'use strict';

var assert = require('assert');
var fs = require('fs');
var path = require('path');
var vm = require('vm');

var project = path.join(__dirname, '..');
var source = fs.readFileSync(path.join(project, 'src', 'pkjs', 'index.js'), 'utf8');
var config = require(path.join(project, 'src', 'pkjs', 'config.js'));
var storage = {};
var listeners = {};
var watchMessages = [];
var requests = [];

function Clay() {}
Clay.prototype.generateUrl = function() { return 'https://clay.example/settings'; };
Clay.prototype.getSettings = function() { return {}; };

function FakeRequest() {
  this.headers = {};
  this.status = 0;
  this.responseText = '';
  requests.push(this);
}
FakeRequest.prototype.open = function(method, url) {
  this.method = method;
  this.url = url;
};
FakeRequest.prototype.setRequestHeader = function(name, value) {
  this.headers[name] = value;
};
FakeRequest.prototype.send = function(body) {
  this.body = body;
};

var sandbox = {
  module: { exports: {} },
  exports: {},
  require: function(name) {
    if (name === '@rebble/clay') {
      return Clay;
    }
    if (name === './config') {
      return config;
    }
    throw new Error('unexpected require ' + name);
  },
  console: { log: function() {} },
  localStorage: {
    getItem: function(key) { return Object.prototype.hasOwnProperty.call(storage, key) ? storage[key] : null; },
    setItem: function(key, value) { storage[key] = String(value); },
    removeItem: function(key) { delete storage[key]; }
  },
  Pebble: {
    addEventListener: function(name, callback) { listeners[name] = callback; },
    sendAppMessage: function(payload, success) {
      watchMessages.push(payload);
      if (success) { success(); }
    },
    openURL: function() {}
  },
  XMLHttpRequest: FakeRequest,
  setTimeout: function() { return 1; },
  clearTimeout: function() {},
  Date: Date,
  Math: Math,
  JSON: JSON,
  Number: Number,
  String: String,
  Boolean: Boolean,
  encodeURIComponent: encodeURIComponent
};

vm.runInNewContext(source, sandbox, { filename: 'src/pkjs/index.js' });
var helpers = sandbox.module.exports;

assert.strictEqual(helpers.normalizeBaseUrl(' https://example.com/// '), 'https://example.com');
assert.strictEqual(helpers.normalizeBaseUrl('http://example.com'), '');
assert(helpers.utf8ByteLength('Привет') > 'Привет'.length);
assert(helpers.utf8ByteLength(helpers.truncateUtf8('я'.repeat(500), 100)) <= 100);

listeners.ready();
assert.strictEqual(watchMessages[watchMessages.length - 1].AGENT_STATE, 0,
  'unconfigured app must tell the watch to open settings');

storage.wrist_agent_settings_v1 = JSON.stringify({
  bridgeUrl: 'https://bridge.example.com',
  deviceToken: 'device-token-12345678901234567890',
  pollSeconds: 4,
  vibrateOnResult: true
});
listeners.ready();
assert.strictEqual(watchMessages[watchMessages.length - 1].AGENT_STATE, 1);

listeners.appmessage({
  payload: {
    TRANSCRIPT: 'Создай напоминание завтра в 15:00',
    COMMAND_NONCE: 42
  }
});
assert.strictEqual(requests.length, 1);
assert.strictEqual(requests[0].method, 'POST');
assert.strictEqual(requests[0].url, 'https://bridge.example.com/v1/requests');
var firstKey = requests[0].headers['Idempotency-Key'];
var pending = JSON.parse(storage.wrist_agent_current_request_v1);
assert.strictEqual(pending.clientRequestId, firstKey);
assert.strictEqual(pending.transcript, 'Создай напоминание завтра в 15:00');

requests[0].ontimeout();
listeners.appmessage({
  payload: {
    TRANSCRIPT: 'Создай напоминание завтра в 15:00',
    COMMAND_NONCE: 42
  }
});
assert.strictEqual(requests.length, 2);
assert.strictEqual(requests[1].headers['Idempotency-Key'], firstKey,
  'retry after an uncertain timeout must reuse the exact idempotency key');
assert.strictEqual(requests[1].body, requests[0].body,
  'retry after an uncertain timeout must reuse the exact request body');

requests[1].status = 202;
requests[1].responseText = JSON.stringify({
  requestId: 'wrq_testrequest1234',
  status: 'queued',
  conversationUrl: 'https://chatgpt.com/c/test'
});
requests[1].onload();
var accepted = JSON.parse(storage.wrist_agent_current_request_v1);
assert.strictEqual(accepted.id, 'wrq_testrequest1234');
assert.strictEqual(accepted.transcript, undefined,
  'accepted transcript must be removed from phone storage');
assert.strictEqual(accepted.commandNonce, '42',
  'watch resend nonce must survive after bridge acceptance');

listeners.appmessage({ payload: { REFRESH_REQUEST: 1 } });
assert.strictEqual(requests.length, 3);
assert.strictEqual(requests[2].method, 'GET');
requests[2].status = 200;
requests[2].responseText = JSON.stringify({
  requestId: 'wrq_testrequest1234',
  status: 'needs_attention',
  canConfirm: true,
  shortAnswer: 'Approval is required.',
  actionSummary: 'Create one reminder at 15:00'
});
requests[2].onload();

listeners.appmessage({ payload: { CONFIRM_REQUEST: 1 } });
assert.strictEqual(requests.length, 4);
var decisionKey = requests[3].headers['Idempotency-Key'];
requests[3].ontimeout();
listeners.appmessage({ payload: { REFRESH_REQUEST: 1 } });
assert.strictEqual(requests.length, 5);
assert.strictEqual(requests[4].headers['Idempotency-Key'], decisionKey,
  'approval retry must reuse the same decision key');

requests[4].status = 409;
requests[4].responseText = JSON.stringify({ error: { code: 'REQUEST_EXPIRED' } });
requests[4].onload();
var afterConflict = JSON.parse(storage.wrist_agent_current_request_v1);
assert.strictEqual(afterConflict.pendingDecision, undefined,
  'terminal decision conflict must stop automatic replay');

listeners.appmessage({ payload: { REFRESH_REQUEST: 1 } });
assert.strictEqual(requests.length, 6);
requests[5].status = 200;
requests[5].responseText = JSON.stringify({
  requestId: 'wrq_testrequest1234',
  status: 'expired',
  errorCode: 'REQUEST_EXPIRED'
});
requests[5].onload();
assert.strictEqual(storage.wrist_agent_current_request_v1, undefined,
  'terminal bridge errors must allow a fresh dictation');
assert.strictEqual(watchMessages[watchMessages.length - 1].RETRY_SAME_REQUEST, undefined);
assert.strictEqual(watchMessages[watchMessages.length - 1].ACTION_SUMMARY, '',
  'every state must explicitly clear stale action text on the watch');

listeners.appmessage({
  payload: { TRANSCRIPT: 'A different request', COMMAND_NONCE: 43 }
});
assert.strictEqual(requests.length, 7);
requests[6].status = 409;
requests[6].responseText = JSON.stringify({ error: { code: 'IDEMPOTENCY_CONFLICT' } });
requests[6].onload();
assert.strictEqual(storage.wrist_agent_current_request_v1, undefined,
  'terminal submission errors must not replay forever');
assert.strictEqual(watchMessages[watchMessages.length - 1].RETRY_SAME_REQUEST, undefined);

assert(helpers.utf8ByteLength(helpers.truncateUtf8('я'.repeat(400), 559)) <= 559,
  'native result payload must reserve one byte for the NUL terminator');

console.log('PebbleKit idempotency and UTF-8 tests passed');

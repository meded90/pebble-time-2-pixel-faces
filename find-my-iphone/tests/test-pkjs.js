'use strict';

var assert = require('assert');
var path = require('path');

var project = path.join(__dirname, '..');
var storage = {};
var listeners = {};
var watchMessages = [];
var openedUrls = [];
var requests = [];

global.localStorage = {
  getItem: function(key) { return Object.prototype.hasOwnProperty.call(storage, key) ? storage[key] : null; },
  setItem: function(key, value) { storage[key] = String(value); },
  removeItem: function(key) { delete storage[key]; }
};

global.Pebble = {
  addEventListener: function(name, callback) { listeners[name] = callback; },
  sendAppMessage: function(payload, success) { watchMessages.push(payload); if (success) success(); },
  openURL: function(url) { openedUrls.push(url); }
};

function FakeRequest() {
  this.headers = {};
  this.responseHeaders = {};
  this.readyState = 0;
  this.status = 0;
  this.responseText = '';
  requests.push(this);
}
FakeRequest.prototype.open = function(method, url) { this.method = method; this.url = url; };
FakeRequest.prototype.setRequestHeader = function(name, value) { this.headers[name] = value; };
FakeRequest.prototype.getAllResponseHeaders = function() { return ''; };
FakeRequest.prototype.getResponseHeader = function(name) { return this.responseHeaders[name] || null; };
FakeRequest.prototype.send = function(body) { this.body = body; };
FakeRequest.prototype.abort = function() {};
global.XMLHttpRequest = FakeRequest;

function respond(request, status, body, headers) {
  request.status = status;
  request.responseText = body === null ? '' : JSON.stringify(body);
  request.responseHeaders = headers || {};
  request.readyState = 4;
  request.onreadystatechange();
}

async function main() {
  var crypto = require(path.join(project, 'src', 'pkjs', 'crypto.js'));
  var configPage = require(path.join(project, 'src', 'pkjs', 'config-page.js'));
  var apple = require(path.join(project, 'src', 'pkjs', 'apple.js'));
  var index = require(path.join(project, 'src', 'pkjs', 'index.js'));

  listeners.ready();
  assert.strictEqual(watchMessages[watchMessages.length - 1].STATE, index.STATE.AUTH_REQUIRED);

  var loginUrl = configPage.generateUrl({ stage: 'login', appleId: 'test@example.com', locale: 'ru_RU' });
  var loginHtml = decodeURIComponent(loginUrl.split(',')[1]);
  assert(loginHtml.indexOf('Подключить Apple Account') !== -1);
  assert(loginHtml.indexOf('value="test@example.com"') !== -1);
  assert(loginHtml.indexOf('temporary-password') === -1);
  assert(loginHtml.indexOf('пароль и код 2FA') !== -1);
  assert.strictEqual(configPage.languageFor('de_DE'), 'de');
  assert.strictEqual(configPage.languageFor('ja_JP'), 'en');
  assert(decodeURIComponent(configPage.generateUrl({ stage: 'login', locale: 'de_DE' }).split(',')[1]).indexOf('Apple Account verbinden') !== -1);
  assert.deepStrictEqual(index.parseConfigurationResponse(encodeURIComponent(JSON.stringify({ action: 'verify', code: '123456' }))), {
    action: 'verify', code: '123456'
  });

  var salt = Array.from({ length: 16 }, function(_, i) { return i + 1; });
  var serverPublic = Array(256).fill(1);
  var privateBytes = Array.from({ length: 32 }, function(_, i) { return i + 33; });
  var proof = crypto.createSrpProof('test@example.com', 'correct horse battery staple', {
    salt: crypto.bytesToBase64(salt),
    b: crypto.bytesToBase64(serverPublic),
    iteration: 1000,
    protocol: 's2k'
  }, privateBytes);
  assert.strictEqual(proof.m1, 'LyQBRxnOLOaKeEB9Wqll/L+3pGQQ6FZ/L8LGL56yLIc=');
  assert.strictEqual(proof.m2, 'ID8+wkW7Ve1UyKlsI2vri/QDoFoAgLDJn/vgU2O2Fww=');

  var session = {
    serviceRoot: 'https://fmip.example.com', clientId: 'client-id', dsid: '12345',
    cookieHeader: 'session=cookie', sessionToken: 'session-token'
  };
  var playPromise = apple.playSound(session, 'iphone-id');
  assert.strictEqual(requests.length, 1);
  assert.strictEqual(requests[0].method, 'POST');
  assert(requests[0].url.indexOf('/fmipservice/client/web/playSound?') !== -1);
  var playBody = JSON.parse(requests[0].body);
  assert.strictEqual(playBody.device, 'iphone-id');
  assert.strictEqual(playBody.clientContext.fmly, true);
  respond(requests[0], 200, { content: [{ snd: { statusCode: '200' } }] });
  var playResult = await playPromise;
  assert.strictEqual(playResult.accepted, true);

  var refreshPromise = apple.refreshDevices(session);
  assert.strictEqual(requests.length, 2);
  assert(requests[1].url.indexOf('/fmipservice/client/web/initClient?') !== -1);
  respond(requests[1], 200, { content: [
    { id: 'iphone-id', name: 'Main iPhone', deviceClass: 'iPhone', features: { SND: true } },
    { id: 'ipad-id', name: 'iPad', deviceClass: 'iPad', features: { SND: true } },
    { id: 'silent-id', name: 'Silent iPhone', deviceClass: 'iPhone', features: {} }
  ] });
  var refreshResult = await refreshPromise;
  assert.deepStrictEqual(refreshResult.devices, [{ id: 'iphone-id', name: 'Main iPhone' }]);

  var retryPromise = apple.playSound(session, 'iphone-id');
  assert.strictEqual(requests.length, 3);
  respond(requests[2], 450, { error: 'session bootstrap required' });
  await Promise.resolve();
  assert.strictEqual(requests.length, 4);
  assert.strictEqual(requests[3].url, 'https://setup.icloud.com/setup/ws/1/accountLogin');
  respond(requests[3], 200, {
    dsInfo: { dsid: '12345', countryCode: 'AE' },
    webservices: { findme: { url: 'https://fmip.example.com:443' } }
  });
  await Promise.resolve();
  await Promise.resolve();
  assert.strictEqual(requests.length, 5);
  assert(requests[4].url.indexOf('/fmipservice/client/web/playSound?') !== -1);
  respond(requests[4], 200, { content: [{ snd: { statusCode: '200' } }] });
  assert.strictEqual((await retryPromise).accepted, true);

  Object.keys(storage).forEach(function(key) {
    assert(storage[key].indexOf('correct horse battery staple') === -1, 'password leaked into ' + key);
    assert(storage[key].indexOf('123456') === -1, '2FA code leaked into ' + key);
  });

  console.log('PebbleKit JS auth, SRP, storage and Find My request tests passed');
}

main().catch(function(error) {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});

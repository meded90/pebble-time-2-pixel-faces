'use strict';

var Clay = require('@rebble/clay');
var config = require('./config');
var clay = new Clay(config, null, { autoHandleEvents: false });

var SETTINGS_KEY = 'wrist_agent_settings_v1';
var CURRENT_REQUEST_KEY = 'wrist_agent_current_request_v1';
var DEFAULT_POLL_SECONDS = 4;
var MAX_WATCH_TEXT_BYTES = 559;
var pollTimer = null;
var activeRequest = null;
var postInFlight = false;
var pollInFlight = false;
var decisionInFlight = false;

var WATCH_STATE = {
  NOT_CONFIGURED: 0,
  READY: 1,
  SENDING: 2,
  WORKING: 3,
  COMPLETED: 4,
  NEEDS_ATTENTION: 5,
  ERROR: 6
};

function unwrapSetting(settings, key) {
  var value = settings[key];
  if (value && typeof value === 'object' && value.value !== undefined) {
    return value.value;
  }
  return value;
}

function loadSettings() {
  var settings = {
    bridgeUrl: '',
    deviceToken: '',
    pollSeconds: DEFAULT_POLL_SECONDS,
    vibrateOnResult: true
  };

  try {
    var stored = JSON.parse(localStorage.getItem(SETTINGS_KEY));
    if (stored) {
      settings.bridgeUrl = stored.bridgeUrl || '';
      settings.deviceToken = stored.deviceToken || '';
      settings.pollSeconds = Number(stored.pollSeconds) || DEFAULT_POLL_SECONDS;
      settings.vibrateOnResult = stored.vibrateOnResult !== false;
    }
  } catch (error) {
    console.log('Wrist Agent settings unavailable: ' + error);
  }

  return settings;
}

function normalizeBaseUrl(value) {
  var url = String(value || '').replace(/^\s+|\s+$/g, '').replace(/\/+$/, '');
  if (!url) {
    return '';
  }
  if (!/^https:\/\//i.test(url) && !/^http:\/\/localhost(?::\d+)?$/i.test(url)) {
    return '';
  }
  return url;
}

function saveSettingsFromResponse(response) {
  var raw = clay.getSettings(response, false);
  var pollSeconds = Number(unwrapSetting(raw, 'POLL_SECONDS')) || DEFAULT_POLL_SECONDS;
  var settings = {
    bridgeUrl: normalizeBaseUrl(unwrapSetting(raw, 'BRIDGE_URL')),
    deviceToken: String(unwrapSetting(raw, 'DEVICE_TOKEN') || '').replace(/^\s+|\s+$/g, ''),
    pollSeconds: Math.max(3, Math.min(10, pollSeconds)),
    vibrateOnResult: unwrapSetting(raw, 'VIBRATE_ON_RESULT') !== false
  };

  localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
  return settings;
}

function isConfigured(settings) {
  return Boolean(normalizeBaseUrl(settings.bridgeUrl) && settings.deviceToken);
}

function utf8ByteLength(value) {
  var text = String(value || '');
  var bytes = 0;
  var index;
  for (index = 0; index < text.length; index += 1) {
    var code = text.charCodeAt(index);
    if (code < 0x80) {
      bytes += 1;
    } else if (code < 0x800) {
      bytes += 2;
    } else if (code >= 0xD800 && code <= 0xDBFF && index + 1 < text.length) {
      bytes += 4;
      index += 1;
    } else {
      bytes += 3;
    }
  }
  return bytes;
}

function truncateUtf8(value, maxBytes) {
  var text = String(value || '');
  if (utf8ByteLength(text) <= maxBytes) {
    return text;
  }

  var result = '';
  var index;
  for (index = 0; index < text.length; index += 1) {
    var character = text.charAt(index);
    var code = text.charCodeAt(index);
    if (code >= 0xD800 && code <= 0xDBFF && index + 1 < text.length) {
      character += text.charAt(index + 1);
      index += 1;
    }
    if (utf8ByteLength(result + character + '...') > maxBytes) {
      break;
    }
    result += character;
  }
  return result + '...';
}

function sendToWatch(payload) {
  Pebble.sendAppMessage(payload, function() {
    console.log('Wrist Agent state sent to watch');
  }, function(error) {
    console.log('Wrist Agent AppMessage failed: ' + JSON.stringify(error));
  });
}

function sendState(state, resultText, actionSummary, errorCode, options) {
  var payload = {
    AGENT_STATE: state,
    RESULT_TEXT: truncateUtf8(resultText || '', MAX_WATCH_TEXT_BYTES),
    ACTION_SUMMARY: truncateUtf8(actionSummary || '', 180),
    ERROR_CODE: truncateUtf8(errorCode || '', 48)
  };
  if (options && options.canConfirm) {
    payload.CAN_CONFIRM = 1;
  }
  if (options && options.retrySame) {
    payload.RETRY_SAME_REQUEST = 1;
  }
  if (options && options.vibrate) {
    payload.VIBRATE_ON_RESULT = 1;
  }
  sendToWatch(payload);
}

function errorMessageForCode(code) {
  var messages = {
    AUTH: 'Check the device token in settings.',
    CONFIG: 'Open phone settings and enter the bridge URL and device token.',
    NETWORK: 'Phone could not reach the bridge.',
    TIMEOUT: 'The bridge did not answer in time.',
    SERVER: 'The bridge reported an error.',
    DATA: 'The bridge returned an invalid response.',
    NOT_FOUND: 'The saved request expired. Ask again.'
  };
  return messages[code] || 'Request failed. Try again.';
}

function fail(code, detail, retrySame) {
  console.log('Wrist Agent error ' + code + ': ' + (detail || ''));
  sendState(WATCH_STATE.ERROR, errorMessageForCode(code), '', code, {
    retrySame: retrySame !== false
  });
}

function parseJson(request) {
  try {
    return JSON.parse(request.responseText || '{}');
  } catch (error) {
    return null;
  }
}

function requestErrorCode(status) {
  if (status === 401 || status === 403) {
    return 'AUTH';
  }
  if (status === 404 || status === 410) {
    return 'NOT_FOUND';
  }
  return 'SERVER';
}

function saveActiveRequest(request) {
  activeRequest = request;
  if (request) {
    localStorage.setItem(CURRENT_REQUEST_KEY, JSON.stringify(request));
  } else {
    localStorage.removeItem(CURRENT_REQUEST_KEY);
  }
}

function loadActiveRequest() {
  try {
    var stored = JSON.parse(localStorage.getItem(CURRENT_REQUEST_KEY));
    if (stored && (stored.id || (stored.clientRequestId && stored.transcript))) {
      activeRequest = stored;
      return stored;
    }
  } catch (error) {
    console.log('Wrist Agent request cache unavailable: ' + error);
  }
  return null;
}

function clearPollTimer() {
  if (pollTimer) {
    clearTimeout(pollTimer);
    pollTimer = null;
  }
}

function schedulePoll(settings, serverDelayMs) {
  clearPollTimer();
  var configuredDelay = settings.pollSeconds * 1000;
  var delay = Number(serverDelayMs) || configuredDelay;
  delay = Math.max(configuredDelay, Math.min(60000, delay));
  pollTimer = setTimeout(function() {
    pollRequest(settings);
  }, delay);
}

function applyRequestStatus(body, settings) {
  var status = body && body.status;
  if (!status) {
    fail('DATA', 'missing status');
    return;
  }

  if (activeRequest) {
    activeRequest.status = status;
    activeRequest.updatedAt = Date.now();
    if (body.conversationUrl) {
      activeRequest.conversationUrl = body.conversationUrl;
    }
    saveActiveRequest(activeRequest);
  }

  if (status === 'completed') {
    clearPollTimer();
    var shouldVibrate = settings.vibrateOnResult && !activeRequest.notifiedCompleted;
    activeRequest.notifiedCompleted = true;
    saveActiveRequest(activeRequest);
    sendState(
      WATCH_STATE.COMPLETED,
      body.shortAnswer || 'Done.',
      body.actionSummary || '',
      '',
      { vibrate: shouldVibrate }
    );
    return;
  }

  if (status === 'needs_attention' || status === 'suspended') {
    clearPollTimer();
    var shouldVibrateAttention = settings.vibrateOnResult && !activeRequest.notifiedAttention;
    activeRequest.notifiedAttention = true;
    saveActiveRequest(activeRequest);
    sendState(
      WATCH_STATE.NEEDS_ATTENTION,
      body.shortAnswer || 'Open the ChatGPT conversation to continue or approve the action.',
      body.actionSummary || '',
      'ATTENTION',
      { canConfirm: body.canConfirm === true, vibrate: shouldVibrateAttention }
    );
    return;
  }

  if (status === 'failed' || status === 'expired') {
    clearPollTimer();
    saveActiveRequest(null);
    sendState(
      WATCH_STATE.ERROR,
      body.shortAnswer || 'The agent could not finish the request.',
      body.actionSummary || '',
      body.errorCode || 'AGENT',
      { retrySame: false }
    );
    return;
  }

  sendState(
    WATCH_STATE.WORKING,
    body.shortAnswer || 'Workspace Agent is working...',
    body.actionSummary || '',
    ''
  );
  schedulePoll(settings, body.pollAfterMs);
}

function pollRequest(settings) {
  clearPollTimer();
  if (activeRequest && activeRequest.pendingDecision) {
    sendDecision(activeRequest.pendingDecision);
    return;
  }
  if (activeRequest && !activeRequest.id && activeRequest.clientRequestId && activeRequest.transcript) {
    postPendingRequest(settings);
    return;
  }
  if (!activeRequest || !activeRequest.id) {
    sendState(WATCH_STATE.READY);
    return;
  }
  if (!isConfigured(settings)) {
    sendState(WATCH_STATE.NOT_CONFIGURED);
    return;
  }
  if (pollInFlight) {
    return;
  }

  var polledRequestId = activeRequest.id;
  var polledGeneration = Number(activeRequest.generation) || 0;
  pollInFlight = true;
  var request = new XMLHttpRequest();
  request.timeout = 15000;
  request.onload = function() {
    pollInFlight = false;
    if (!activeRequest || activeRequest.id !== polledRequestId ||
        (Number(activeRequest.generation) || 0) !== polledGeneration) {
      return;
    }
    if (request.status < 200 || request.status >= 300) {
      var code = requestErrorCode(request.status);
      if (code === 'NOT_FOUND') {
        saveActiveRequest(null);
      }
      fail(code, 'poll HTTP ' + request.status, code !== 'NOT_FOUND');
      return;
    }
    var body = parseJson(request);
    if (!body) {
      fail('DATA', 'poll JSON');
      return;
    }
    applyRequestStatus(body, settings);
  };
  request.onerror = function() {
    pollInFlight = false;
    if (!activeRequest || activeRequest.id !== polledRequestId ||
        (Number(activeRequest.generation) || 0) !== polledGeneration) {
      return;
    }
    fail('NETWORK', 'poll network');
  };
  request.ontimeout = function() {
    pollInFlight = false;
    if (!activeRequest || activeRequest.id !== polledRequestId ||
        (Number(activeRequest.generation) || 0) !== polledGeneration) {
      return;
    }
    fail('TIMEOUT', 'poll timeout');
  };
  request.open('GET', normalizeBaseUrl(settings.bridgeUrl) + '/v1/requests/' + encodeURIComponent(activeRequest.id));
  request.setRequestHeader('Authorization', 'Bearer ' + settings.deviceToken);
  request.send();
}

function makeClientRequestId() {
  return 'pebble-' + Date.now().toString(36) + '-' + Math.floor(Math.random() * 0x1000000).toString(36);
}

function postPendingRequest(settings) {
  if (postInFlight || !activeRequest || !activeRequest.clientRequestId ||
      !activeRequest.transcript || activeRequest.id) {
    return;
  }
  if (!isConfigured(settings)) {
    sendState(WATCH_STATE.NOT_CONFIGURED);
    return;
  }

  postInFlight = true;
  clearPollTimer();
  sendState(WATCH_STATE.SENDING, 'Sending accepted request...');
  var request = new XMLHttpRequest();
  request.timeout = 20000;
  request.onload = function() {
    postInFlight = false;
    if (request.status < 200 || request.status >= 300) {
      var terminalSubmission = request.status === 400 || request.status === 409 ||
        request.status === 413;
      if (terminalSubmission) {
        saveActiveRequest(null);
      }
      fail(
        requestErrorCode(request.status),
        'submit HTTP ' + request.status,
        !terminalSubmission
      );
      return;
    }
    var body = parseJson(request);
    if (!body || !body.requestId) {
      fail('DATA', 'submit JSON');
      return;
    }
    saveActiveRequest({
      id: body.requestId,
      status: body.status || 'queued',
      generation: Number(activeRequest.generation) || 1,
      commandNonce: activeRequest.commandNonce || '',
      createdAt: activeRequest.createdAt || Date.now(),
      conversationUrl: body.conversationUrl || ''
    });
    applyRequestStatus(body, settings);
  };
  request.onerror = function() {
    postInFlight = false;
    fail('NETWORK', 'submit network');
  };
  request.ontimeout = function() {
    postInFlight = false;
    fail('TIMEOUT', 'submit timeout');
  };
  request.open('POST', normalizeBaseUrl(settings.bridgeUrl) + '/v1/requests');
  request.setRequestHeader('Authorization', 'Bearer ' + settings.deviceToken);
  request.setRequestHeader('Content-Type', 'application/json');
  request.setRequestHeader('Idempotency-Key', activeRequest.clientRequestId);
  request.send(JSON.stringify({
    command: activeRequest.transcript,
    utcOffsetMinutes: activeRequest.utcOffsetMinutes,
    clientRequestId: activeRequest.clientRequestId,
    source: 'pebble-time-2'
  }));
}

function submitTranscript(transcript, commandNonce) {
  var settings = loadSettings();
  if (!isConfigured(settings)) {
    sendState(WATCH_STATE.NOT_CONFIGURED);
    return;
  }

  var text = String(transcript || '').replace(/^\s+|\s+$/g, '');
  if (!text) {
    fail('DATA', 'empty transcript');
    return;
  }

  var normalizedNonce = String(commandNonce === undefined ? '' : commandNonce);
  if (normalizedNonce && activeRequest && activeRequest.commandNonce === normalizedNonce) {
    sendState(WATCH_STATE.WORKING, 'Restoring the accepted request...');
    if (activeRequest.id) {
      pollRequest(settings);
    } else {
      postPendingRequest(settings);
    }
    return;
  }

  saveActiveRequest({
    id: null,
    status: 'submitting',
    clientRequestId: makeClientRequestId(),
    transcript: text,
    commandNonce: normalizedNonce,
    utcOffsetMinutes: -new Date().getTimezoneOffset(),
    generation: 1,
    createdAt: Date.now()
  });
  postPendingRequest(settings);
}

function sendDecision(decision) {
  var settings = loadSettings();
  if (!activeRequest || !activeRequest.id || !isConfigured(settings)) {
    fail('NOT_FOUND', 'decision without request');
    return;
  }
  if (decisionInFlight) {
    return;
  }

  clearPollTimer();
  sendState(WATCH_STATE.WORKING,
    decision === 'approve' ? 'Sending your approval...' : 'Cancelling the proposed action...');
  if (!activeRequest.pendingDecision) {
    activeRequest.generation = (Number(activeRequest.generation) || 0) + 1;
  }
  var decisionKey = activeRequest.decisionKey || makeClientRequestId();
  activeRequest.decisionKey = decisionKey;
  activeRequest.pendingDecision = decision;
  saveActiveRequest(activeRequest);
  var decidedRequestId = activeRequest.id;
  var decidedGeneration = Number(activeRequest.generation) || 0;

  decisionInFlight = true;
  var request = new XMLHttpRequest();
  request.timeout = 20000;
  request.onload = function() {
    decisionInFlight = false;
    if (!activeRequest || activeRequest.id !== decidedRequestId ||
        (Number(activeRequest.generation) || 0) !== decidedGeneration) {
      return;
    }
    if (request.status < 200 || request.status >= 300) {
      if (request.status === 404 || request.status === 409) {
        delete activeRequest.decisionKey;
        delete activeRequest.pendingDecision;
        saveActiveRequest(activeRequest);
        if (request.status === 404) {
          saveActiveRequest(null);
          fail('NOT_FOUND', 'decision HTTP 404', false);
        } else {
          schedulePoll(settings, 0);
        }
        return;
      }
      fail(requestErrorCode(request.status), 'approval HTTP ' + request.status);
      return;
    }
    var body = parseJson(request);
    if (!body || !body.requestId) {
      fail('DATA', 'decision JSON');
      return;
    }
    delete activeRequest.decisionKey;
    delete activeRequest.pendingDecision;
    saveActiveRequest(activeRequest);
    applyRequestStatus(body, settings);
  };
  request.onerror = function() {
    decisionInFlight = false;
    if (!activeRequest || activeRequest.id !== decidedRequestId ||
        (Number(activeRequest.generation) || 0) !== decidedGeneration) {
      return;
    }
    fail('NETWORK', 'decision network');
  };
  request.ontimeout = function() {
    decisionInFlight = false;
    if (!activeRequest || activeRequest.id !== decidedRequestId ||
        (Number(activeRequest.generation) || 0) !== decidedGeneration) {
      return;
    }
    fail('TIMEOUT', 'decision timeout');
  };
  request.open('POST', normalizeBaseUrl(settings.bridgeUrl) + '/v1/requests/' +
    encodeURIComponent(activeRequest.id) + '/decision');
  request.setRequestHeader('Authorization', 'Bearer ' + settings.deviceToken);
  request.setRequestHeader('Content-Type', 'application/json');
  request.setRequestHeader('Idempotency-Key', decisionKey);
  request.send(JSON.stringify({ decision: decision }));
}

function announceReady() {
  var settings = loadSettings();
  if (!isConfigured(settings)) {
    sendState(WATCH_STATE.NOT_CONFIGURED);
    return;
  }

  if (!activeRequest) {
    loadActiveRequest();
  }
  if (activeRequest) {
    sendState(WATCH_STATE.WORKING, 'Restoring the last request...');
    if (activeRequest.pendingDecision) {
      sendDecision(activeRequest.pendingDecision);
    } else {
      pollRequest(settings);
    }
  } else {
    sendState(WATCH_STATE.READY);
  }
}

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(event) {
  if (!event || !event.response) {
    return;
  }
  try {
    saveSettingsFromResponse(event.response);
    announceReady();
  } catch (error) {
    fail('DATA', 'settings response ' + error);
  }
});

Pebble.addEventListener('ready', function() {
  loadActiveRequest();
  announceReady();
});

Pebble.addEventListener('appmessage', function(event) {
  if (event.payload.TRANSCRIPT) {
    submitTranscript(event.payload.TRANSCRIPT, event.payload.COMMAND_NONCE);
    return;
  }
  if (event.payload.REFRESH_REQUEST) {
    pollRequest(loadSettings());
    return;
  }
  if (event.payload.CONFIRM_REQUEST) {
    sendDecision(Number(event.payload.CONFIRM_REQUEST) === 2 ? 'reject' : 'approve');
  }
});

module.exports = {
  normalizeBaseUrl: normalizeBaseUrl,
  truncateUtf8: truncateUtf8,
  utf8ByteLength: utf8ByteLength
};

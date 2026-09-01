'use strict';

require('es6-promise').polyfill();

var apple = require('./apple');
var configPage = require('./config-page');

var SESSION_KEY = 'find_my_iphone_session_v1';
var PENDING_KEY = 'find_my_iphone_pending_auth_v1';
var DEVICES_KEY = 'find_my_iphone_devices_v1';
var SETTINGS_KEY = 'find_my_iphone_preferences_v1';
var CONFIG_STAGE_KEY = 'find_my_iphone_config_stage_v1';
var CONFIG_ERROR_KEY = 'find_my_iphone_config_error_v1';
var COOLDOWN_MS = 10000;

var STATE = {
  BOOT: 0,
  AUTH_REQUIRED: 1,
  READY: 2,
  ARMING: 3,
  REQUESTING: 4,
  RINGING_ACCEPTED: 5,
  OFFLINE: 6,
  AUTH_EXPIRED: 7,
  RATE_LIMITED: 8,
  ERROR: 9
};
var COMMAND = { PLAY_SOUND: 1, REFRESH_STATUS: 2, SELECT_DEVICE: 3 };
var session = readJson(SESSION_KEY);
var pendingAuth = readJson(PENDING_KEY);
var devices = readJson(DEVICES_KEY) || [];
var preferences = readJson(SETTINGS_KEY) || {};
var configStage = readJson(CONFIG_STAGE_KEY) || '';
var inFlight = false;
var lastPlayAt = 0;
var lastError = readJson(CONFIG_ERROR_KEY) || '';

function readJson(key) {
  try { return JSON.parse(localStorage.getItem(key)); } catch (error) { localStorage.removeItem(key); return null; }
}

function writeJson(key, value) {
  if (value === null || value === undefined) localStorage.removeItem(key);
  else localStorage.setItem(key, JSON.stringify(value));
}

function safeError(error) {
  var code = String(error && error.message || error || 'UNKNOWN');
  return code.replace(/[^A-Z0-9_ -]/gi, '').slice(0, 72);
}

function validConfigStage(stage) {
  return ['login', 'two-factor', 'devices', 'connected'].indexOf(stage) !== -1;
}

function setConfigStage(stage) {
  configStage = validConfigStage(stage) ? stage : '';
  writeJson(CONFIG_STAGE_KEY, configStage || null);
}

function setConfigError(error) {
  lastError = error ? safeError(error) : '';
  writeJson(CONFIG_ERROR_KEY, lastError || null);
}

function selectedDevice() {
  var selected = devices.filter(function(device) { return device.id === preferences.selectedDeviceId; })[0];
  return selected || devices[0] || null;
}

function sendState(state, options) {
  options = options || {};
  var selected = selectedDevice();
  var payload = {
    STATE: state,
    RESULT: options.result || 0,
    ERROR_CODE: options.errorCode || 0,
    DEVICE_COUNT: Math.min(devices.length, 255),
    DEVICE_INDEX: Math.max(0, devices.indexOf(selected)),
    DEVICE_NAME: options.deviceName !== undefined ? options.deviceName : (selected ? selected.name.slice(0, 31) : ''),
    SELECTED_DEVICE: selected ? selected.id.slice(0, 63) : '',
    REQUEST_ID: options.requestId || 0
  };
  Pebble.sendAppMessage(payload, function() {}, function(event) {
    console.log('Find My state delivery failed: ' + JSON.stringify(event && event.error || {}));
  });
}

function persistReadySession(nextSession, nextDevices) {
  session = Object.assign({}, nextSession);
  delete session.needs2FA;
  pendingAuth = null;
  devices = nextDevices || devices;
  if (!preferences.selectedDeviceId || !devices.some(function(device) { return device.id === preferences.selectedDeviceId; })) {
    preferences.selectedDeviceId = devices[0] ? devices[0].id : '';
  }
  writeJson(SESSION_KEY, session);
  writeJson(PENDING_KEY, null);
  writeJson(DEVICES_KEY, devices);
  writeJson(SETTINGS_KEY, preferences);
}

function errorState(error, requestId) {
  var code = safeError(error);
  setConfigError(code);
  if (code.indexOf('AUTH_EXPIRED') === 0) {
    sendState(STATE.AUTH_EXPIRED, { errorCode: 401, requestId: requestId });
  } else if (code.indexOf('RATE_LIMITED') === 0) {
    sendState(STATE.RATE_LIMITED, { errorCode: 429, requestId: requestId });
  } else if (code.indexOf('NETWORK') === 0) {
    sendState(STATE.OFFLINE, { errorCode: 1, requestId: requestId });
  } else {
    sendState(STATE.ERROR, { errorCode: 500, requestId: requestId });
  }
}

function refreshStatus(options) {
  options = options || {};
  if (!session || !session.sessionToken || pendingAuth) {
    sendState(STATE.AUTH_REQUIRED);
    return Promise.resolve(false);
  }
  if (inFlight) return Promise.resolve(false);
  inFlight = true;
  if (!options.quiet) sendState(STATE.REQUESTING);
  return apple.refreshDevices(session).then(function(result) {
    persistReadySession(result.session, result.devices);
    if (devices.length) sendState(STATE.READY);
    else sendState(STATE.ERROR, { errorCode: 404, deviceName: '' });
    return devices.length > 0;
  }).catch(function(error) {
    errorState(error, 0);
    return false;
  }).then(function(result) {
    inFlight = false;
    return result;
  });
}

function playSound(requestId) {
  var now = Date.now();
  if (requestId && requestId === Number(preferences.lastAcceptedRequestId || 0)) {
    sendState(STATE.RINGING_ACCEPTED, { result: 1, requestId: requestId });
    return;
  }
  if (now - lastPlayAt < COOLDOWN_MS) {
    sendState(STATE.RATE_LIMITED, { errorCode: 429, requestId: requestId });
    setTimeout(function() { if (session && devices.length) sendState(STATE.READY); }, COOLDOWN_MS - (now - lastPlayAt));
    return;
  }
  if (inFlight) return;
  var target = selectedDevice();
  if (!session || !target) {
    sendState(STATE.AUTH_REQUIRED, { requestId: requestId });
    return;
  }
  inFlight = true;
  lastPlayAt = now;
  sendState(STATE.REQUESTING, { requestId: requestId });
  apple.playSound(session, target.id).then(function(result) {
    session = result.session;
    writeJson(SESSION_KEY, session);
    if (requestId) {
      preferences.lastAcceptedRequestId = requestId;
      writeJson(SETTINGS_KEY, preferences);
    }
    sendState(STATE.RINGING_ACCEPTED, { result: 1, requestId: requestId });
  }).catch(function(error) {
    errorState(error, requestId);
  }).then(function() { inFlight = false; });
}

function selectById(deviceId) {
  if (devices.some(function(device) { return device.id === deviceId; })) {
    preferences.selectedDeviceId = deviceId;
    writeJson(SETTINGS_KEY, preferences);
  }
  sendState(devices.length ? STATE.READY : STATE.AUTH_REQUIRED);
}

function selectByIndex(index) {
  if (devices[index]) {
    preferences.selectedDeviceId = devices[index].id;
    writeJson(SETTINGS_KEY, preferences);
  }
  sendState(devices.length ? STATE.READY : STATE.AUTH_REQUIRED);
}

function configModel(forceStage) {
  var selected = selectedDevice();
  var derivedStage = pendingAuth ? 'two-factor' : (!session ? 'login' : (!devices.length ? 'devices' : 'connected'));
  var stage = forceStage || configStage || derivedStage;
  if (!validConfigStage(stage) || (stage === 'two-factor' && !pendingAuth) ||
      ((stage === 'devices' || stage === 'connected') && !session)) stage = derivedStage;
  if (stage === 'connected' && !devices.length) stage = 'devices';
  return {
    stage: stage,
    error: lastError,
    appleId: session && session.appleId || pendingAuth && pendingAuth.appleId || '',
    devices: devices,
    selectedDeviceId: selected && selected.id || '',
    selectedDeviceName: selected && selected.name || '',
    locale: preferences.locale || ''
  };
}

function openConfiguration(forceStage) {
  Pebble.openURL(configPage.generateUrl(configModel(forceStage)));
}

function clearSession() {
  var locale = preferences.locale || '';
  session = null;
  pendingAuth = null;
  devices = [];
  preferences = locale ? { locale: locale } : {};
  setConfigStage('login');
  setConfigError('');
  writeJson(SESSION_KEY, null);
  writeJson(PENDING_KEY, null);
  writeJson(DEVICES_KEY, null);
  writeJson(SETTINGS_KEY, locale ? preferences : null);
  sendState(STATE.AUTH_REQUIRED);
}

function parseConfigurationResponse(response) {
  if (!response) return null;
  try { return JSON.parse(decodeURIComponent(response)); } catch (error) {
    try { return JSON.parse(response); } catch (ignored) { return null; }
  }
}

Pebble.addEventListener('ready', function() {
  if (pendingAuth) sendState(STATE.AUTH_REQUIRED);
  else if (session) refreshStatus({ quiet: true });
  else sendState(STATE.AUTH_REQUIRED);
});

Pebble.addEventListener('showConfiguration', function() {
  openConfiguration();
});

Pebble.addEventListener('webviewclosed', function(event) {
  var response = parseConfigurationResponse(event && event.response);
  if (!response || !response.action) return;
  if (response.locale) {
    preferences.locale = String(response.locale).slice(0, 16);
    writeJson(SETTINGS_KEY, preferences);
  }
  setConfigError('');
  if (response.action === 'logout') {
    clearSession();
    return;
  }
  if (response.action === 'devices') {
    setConfigStage('devices');
    return;
  }
  if (response.action === 'refresh') {
    setConfigStage('devices');
    refreshStatus();
    return;
  }
  if (response.action === 'select') {
    selectById(String(response.deviceId || ''));
    setConfigStage('connected');
    return;
  }
  if (response.action === 'login') {
    setConfigStage('login');
    var password = String(response.password || '');
    response.password = '';
    sendState(STATE.REQUESTING);
    apple.startLogin(response.appleId, password, session).then(function(result) {
      password = '';
      if (result.needs2FA) {
        pendingAuth = result.session;
        writeJson(PENDING_KEY, pendingAuth);
        setConfigStage('two-factor');
        sendState(STATE.AUTH_REQUIRED);
        return;
      }
      session = result.session;
      writeJson(SESSION_KEY, session);
      return refreshStatus().then(function() { setConfigStage('devices'); });
    }).catch(function(error) {
      password = '';
      setConfigError(error);
      console.log('Find My login failed: ' + lastError);
      setConfigStage('login');
      sendState(STATE.AUTH_REQUIRED);
    });
    return;
  }
  if (response.action === 'verify') {
    setConfigStage('two-factor');
    var code = String(response.code || '');
    response.code = '';
    sendState(STATE.REQUESTING);
    apple.verifyTwoFactor(pendingAuth, code).then(function(readySession) {
      code = '';
      session = readySession;
      pendingAuth = null;
      writeJson(SESSION_KEY, session);
      writeJson(PENDING_KEY, null);
      return refreshStatus().then(function() { setConfigStage('devices'); });
    }).catch(function(error) {
      code = '';
      setConfigError(error);
      console.log('Find My 2FA failed: ' + lastError);
      setConfigStage('two-factor');
      sendState(STATE.AUTH_REQUIRED);
    });
  }
});

Pebble.addEventListener('appmessage', function(event) {
  var payload = event.payload || {};
  if (payload.LOCALE) {
    preferences.locale = String(payload.LOCALE).slice(0, 16);
    writeJson(SETTINGS_KEY, preferences);
  }
  if (payload.COMMAND === COMMAND.PLAY_SOUND) playSound(Number(payload.REQUEST_ID || 0));
  else if (payload.COMMAND === COMMAND.REFRESH_STATUS) refreshStatus({ quiet: true });
  else if (payload.COMMAND === COMMAND.SELECT_DEVICE) {
    if (payload.DEVICE_INDEX !== undefined) selectByIndex(Number(payload.DEVICE_INDEX));
    else selectById(String(payload.SELECTED_DEVICE || ''));
  }
});

module.exports = {
  COMMAND: COMMAND,
  STATE: STATE,
  configModel: configModel,
  parseConfigurationResponse: parseConfigurationResponse,
  safeError: safeError
};

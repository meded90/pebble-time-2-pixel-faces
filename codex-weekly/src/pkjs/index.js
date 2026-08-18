var Clay = require('@rebble/clay');
var config = require('./config');
var clay = new Clay(config, null, { autoHandleEvents: false });

var SETTINGS_KEY = 'codex_weekly_bridge_settings';
var CACHE_KEY = 'codex_weekly_status_cache';
var refreshTimer = null;

function loadSettings() {
  var fallback = {
    bridgeUrl: '',
    bridgeToken: '',
    refreshMinutes: 30
  };

  try {
    var stored = JSON.parse(localStorage.getItem(SETTINGS_KEY));
    if (stored) {
      fallback.bridgeUrl = stored.bridgeUrl || '';
      fallback.bridgeToken = stored.bridgeToken || '';
      fallback.refreshMinutes = parseInt(stored.refreshMinutes, 10) || 30;
    }
  } catch (error) {
    console.log('Codex settings unavailable: ' + error);
  }

  return fallback;
}

function unwrapSetting(settings, key) {
  var value = settings[key];
  if (value && typeof value === 'object' && value.value !== undefined) {
    return value.value;
  }
  return value;
}

function saveSettingsFromResponse(response) {
  var raw = clay.getSettings(response, false);
  var settings = {
    bridgeUrl: unwrapSetting(raw, 'BRIDGE_URL') || '',
    bridgeToken: unwrapSetting(raw, 'BRIDGE_TOKEN') || '',
    refreshMinutes: parseInt(unwrapSetting(raw, 'REFRESH_MINUTES'), 10) || 30
  };

  localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
  return settings;
}

function normalizeStatusUrl(value) {
  var url = (value || '').replace(/^\s+|\s+$/g, '');
  if (!url) {
    return '';
  }
  if (!/\/status(?:[?#]|$)/.test(url)) {
    url = url.replace(/\/+$/, '') + '/status';
  }
  return url;
}

function formatReset(resetsAt) {
  var remaining = Math.max(0, Number(resetsAt) - Math.floor(Date.now() / 1000));
  var days = Math.floor(remaining / 86400);
  var hours = Math.floor((remaining % 86400) / 3600);
  var dayText = days > 9 ? '9+' : String(days);
  var hourText = hours < 10 ? '0' + hours : String(hours);
  return dayText + 'D ' + hourText + 'H';
}

function sanitizeActivity(value) {
  var activity = typeof value === 'string' ? value.replace(/[^0-4]/g, '') : '';
  if (activity.length !== 84) {
    return null;
  }
  return activity;
}

function formatRawCost(value) {
  var cost = Number(value);
  if (isNaN(cost) || !isFinite(cost) || cost < 0) {
    return '-';
  }
  return '$' + cost.toFixed(2);
}

function sendStatus(payload) {
  Pebble.sendAppMessage(payload, function() {
    console.log('Codex status sent to watch');
  }, function(error) {
    console.log('Codex status send failed: ' + JSON.stringify(error));
  });
}

function sendSyncState(state) {
  sendStatus({ SYNC_STATE: state });
}

function cacheAndSend(result) {
  var weekly = result.weekly || {};
  var activity = result.activity || {};
  var activityMap = sanitizeActivity(activity.levels);
  var payload = {
    SYNC_STATE: 2,
    RAW_COST_TEXT: formatRawCost(weekly.rawCostUsd)
  };

  var rawLeft = Number(weekly.leftPercent);
  if (!isNaN(rawLeft) && isFinite(rawLeft)) {
    var resetsAt = Number(weekly.resetsAt);
    payload.WEEK_LEFT = Math.max(0, Math.min(100, Math.round(rawLeft)));
    payload.RESET_TEXT = !isNaN(resetsAt) && isFinite(resetsAt)
      ? formatReset(resetsAt)
      : '-';
  }
  if (activityMap) {
    payload.ACTIVITY_MAP = activityMap;
  }

  localStorage.setItem(CACHE_KEY, JSON.stringify({
    payload: payload,
    savedAt: Date.now()
  }));
  sendStatus(payload);
}

function sendCachedStatus() {
  try {
    var cached = JSON.parse(localStorage.getItem(CACHE_KEY));
    if (cached && cached.payload) {
      sendStatus(cached.payload);
      return true;
    }
  } catch (error) {
    console.log('Codex cache unavailable: ' + error);
  }
  return false;
}

function scheduleNext(settings) {
  if (refreshTimer) {
    clearTimeout(refreshTimer);
  }
  refreshTimer = setTimeout(function() {
    fetchCodexStatus(settings);
  }, settings.refreshMinutes * 60 * 1000);
}

function fetchCodexStatus(settings) {
  var url = normalizeStatusUrl(settings.bridgeUrl);
  if (!url) {
    sendSyncState(0);
    return;
  }

  sendSyncState(1);
  var request = new XMLHttpRequest();
  request.timeout = 15000;
  request.onload = function() {
    if (request.status < 200 || request.status >= 300) {
      console.log('Codex bridge HTTP status: ' + request.status);
      sendSyncState(3);
      scheduleNext(settings);
      return;
    }

    try {
      cacheAndSend(JSON.parse(request.responseText));
    } catch (error) {
      console.log('Codex bridge response error: ' + error);
      sendSyncState(3);
    }
    scheduleNext(settings);
  };
  request.onerror = function() {
    console.log('Codex bridge request failed');
    sendSyncState(3);
    scheduleNext(settings);
  };
  request.ontimeout = request.onerror;
  request.open('GET', url);
  if (settings.bridgeToken) {
    request.setRequestHeader('Authorization', 'Bearer ' + settings.bridgeToken);
  }
  request.send();
}

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(event) {
  if (!event || !event.response) {
    return;
  }

  try {
    fetchCodexStatus(saveSettingsFromResponse(event.response));
  } catch (error) {
    console.log('Codex settings response error: ' + error);
    sendSyncState(3);
  }
});

Pebble.addEventListener('ready', function() {
  var settings = loadSettings();
  sendCachedStatus();
  fetchCodexStatus(settings);
});

Pebble.addEventListener('appmessage', function(event) {
  if (event.payload.REQUEST_SYNC) {
    fetchCodexStatus(loadSettings());
  }
});

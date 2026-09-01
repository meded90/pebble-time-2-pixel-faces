'use strict';

var srp = require('./crypto');

var AUTH_ROOT = 'https://idmsa.apple.com/appleauth/auth';
var SETUP_ROOT = 'https://setup.icloud.com/setup/ws/1';
var WIDGET_KEY = 'd39ba9916b7251055b22c7f910e2ea796ee65e98b2ddecea8f5dde8d9d1a815d';
var REQUEST_TIMEOUT_MS = 20000;
var FMIP_PARAMS = {
  clientBuildNumber: '2534Project66',
  clientMasteringNumber: '2534B22'
};
var cookieJar = {};

function randomClientId() {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(character) {
    var random = Math.floor(Math.random() * 16);
    var value = character === 'x' ? random : (random & 3) | 8;
    return value.toString(16);
  });
}

function srpEntropy(authorizeResponse) {
  var accountToken = '';
  try {
    if (typeof Pebble !== 'undefined' && typeof Pebble.getAccountToken === 'function') {
      accountToken = String(Pebble.getAccountToken() || '');
    }
  } catch (error) { accountToken = ''; }
  return [
    accountToken,
    authorizeResponse.headers.scnt || '',
    authorizeResponse.headers['set-cookie'] || '',
    cookieHeader(),
    String(Date.now()),
    String(Math.random()),
    String(Math.random())
  ].join('|');
}

function buildQuery(params) {
  return Object.keys(params || {}).filter(function(key) {
    return params[key] !== undefined && params[key] !== '';
  }).map(function(key) {
    return encodeURIComponent(key) + '=' + encodeURIComponent(String(params[key]));
  }).join('&');
}

function parseHeaders(xhr) {
  var headers = {};
  var raw = '';
  try { raw = xhr.getAllResponseHeaders() || ''; } catch (error) { raw = ''; }
  raw.split(/\r?\n/).forEach(function(line) {
    var separator = line.indexOf(':');
    if (separator > 0) headers[line.slice(0, separator).trim().toLowerCase()] = line.slice(separator + 1).trim();
  });
  [
    'Set-Cookie', 'X-Apple-ID-Account-Country', 'X-Apple-ID-Session-Id',
    'X-Apple-Auth-Attributes', 'X-Apple-Session-Token',
    'X-Apple-TwoSV-Trust-Token', 'scnt'
  ].forEach(function(name) {
    var key = name.toLowerCase();
    if (headers[key]) return;
    try { headers[key] = xhr.getResponseHeader(name) || ''; } catch (error) { headers[key] = ''; }
  });
  return headers;
}

function mergeCookies(header) {
  if (!header) return;
  String(header).split(/,\s*(?=[A-Za-z_-]+=)/).forEach(function(cookie) {
    var nameValue = cookie.split(';')[0].trim();
    var separator = nameValue.indexOf('=');
    if (separator > 0) cookieJar[nameValue.slice(0, separator)] = nameValue;
  });
}

function cookieHeader() {
  return Object.keys(cookieJar).map(function(name) { return cookieJar[name]; }).join('; ');
}

function restoreCookies(header) {
  cookieJar = {};
  String(header || '').split(/;\s*/).forEach(function(cookie) {
    var separator = cookie.indexOf('=');
    if (separator > 0) cookieJar[cookie.slice(0, separator)] = cookie;
  });
}

function requestJson(method, url, params, headers, body) {
  return new Promise(function(resolve, reject) {
    var xhr = new XMLHttpRequest();
    var query = buildQuery(params);
    var requestUrl = query ? url + '?' + query : url;
    var settled = false;
    var timer = setTimeout(function() {
      if (settled) return;
      settled = true;
      try { xhr.abort(); } catch (error) { /* Older PKJS runtimes may not expose abort. */ }
      reject(new Error('NETWORK_TIMEOUT'));
    }, REQUEST_TIMEOUT_MS);

    function finish(value, error) {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      if (error) reject(error); else resolve(value);
    }

    xhr.open(method, requestUrl, true);
    xhr.timeout = REQUEST_TIMEOUT_MS;
    xhr.withCredentials = true;
    var cookies = cookieHeader();
    if (cookies) {
      try { xhr.setRequestHeader('Cookie', cookies); } catch (error) { /* Native jar may handle it. */ }
    }
    Object.keys(headers || {}).forEach(function(name) {
      try { xhr.setRequestHeader(name, headers[name]); } catch (error) { /* Restricted native header. */ }
    });
    xhr.onreadystatechange = function() {
      if (xhr.readyState !== 4) return;
      var text = xhr.responseText || '';
      var parsed = null;
      try { parsed = text ? JSON.parse(text) : null; } catch (error) { parsed = null; }
      var responseHeaders = parseHeaders(xhr);
      mergeCookies(responseHeaders['set-cookie']);
      finish({ status: xhr.status, body: parsed, text: text, headers: responseHeaders });
    };
    xhr.onerror = function() { finish(null, new Error('NETWORK')); };
    xhr.ontimeout = function() { finish(null, new Error('NETWORK_TIMEOUT')); };
    xhr.send(body === undefined ? null : JSON.stringify(body));
  });
}

function authHeaders(clientId, session) {
  var headers = {
    Accept: 'application/json, text/javascript',
    'Content-Type': 'application/json',
    Referer: 'https://idmsa.apple.com',
    'X-Apple-OAuth-Client-Id': WIDGET_KEY,
    'X-Apple-OAuth-Client-Type': 'firstPartyAuth',
    'X-Apple-OAuth-Redirect-URI': 'https://www.icloud.com',
    'X-Apple-OAuth-Require-Grant-Code': 'true',
    'X-Apple-OAuth-Response-Mode': 'web_message',
    'X-Apple-OAuth-Response-Type': 'code',
    'X-Apple-OAuth-State': clientId,
    'X-Apple-Frame-Id': clientId,
    'X-Apple-Widget-Key': WIDGET_KEY,
    'X-Apple-FD-Client-Info': JSON.stringify({
      U: 'Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15',
      L: 'en-US', Z: 'GMT+04:00', V: '1.1', F: ''
    })
  };
  if (session && session.scnt) headers.scnt = session.scnt;
  if (session && session.sessionId) headers['X-Apple-ID-Session-Id'] = session.sessionId;
  if (session && session.authAttributes) headers['X-Apple-Auth-Attributes'] = session.authAttributes;
  return headers;
}

function withHeaderSession(session, headers) {
  return Object.assign({}, session, {
    accountCountry: String(headers['x-apple-id-account-country'] || session.accountCountry || ''),
    sessionId: String(headers['x-apple-id-session-id'] || session.sessionId || ''),
    authAttributes: String(headers['x-apple-auth-attributes'] || session.authAttributes || ''),
    sessionToken: String(headers['x-apple-session-token'] || session.sessionToken || ''),
    trustToken: String(headers['x-apple-twosv-trust-token'] || session.trustToken || ''),
    scnt: String(headers.scnt || session.scnt || ''),
    cookieHeader: cookieHeader()
  });
}

function appleError(response) {
  var body = response && response.body || {};
  var errors = Array.isArray(body.serviceErrors) ? body.serviceErrors : [];
  var first = errors[0] || {};
  var status = response ? response.status : 0;
  var code = first.code || first.errorCode || body.errorCode || body.code || '';
  var message = first.message || first.errorMessage || body.errorMessage || body.reason || body.error || '';
  return ['HTTP', status, code, message].filter(function(value) {
    return value !== '' && value !== null && value !== undefined;
  }).join('_');
}

function accountLogin(session) {
  restoreCookies(session.cookieHeader);
  return requestJson('POST', SETUP_ROOT + '/accountLogin', {}, {
    Accept: 'application/json, text/javascript',
    'Content-Type': 'application/json',
    Origin: 'https://www.icloud.com',
    Referer: 'https://www.icloud.com/'
  }, {
    accountCountryCode: session.accountCountry || '',
    dsWebAuthToken: session.sessionToken,
    extended_login: true,
    trustToken: session.trustToken || ''
  }).then(function(response) {
    if (response.status !== 200 || !response.body) throw new Error('ACCOUNT_LOGIN_' + appleError(response));
    var next = withHeaderSession(session, response.headers);
    var findme = response.body.webservices && response.body.webservices.findme || {};
    var dsInfo = response.body.dsInfo || {};
    next.serviceRoot = String(findme.url || next.serviceRoot || '').replace(/:443$/, '');
    next.dsid = String(dsInfo.dsid || next.dsid || '');
    next.accountCountry = String(next.accountCountry || dsInfo.countryCode || '');
    next.updatedAt = new Date().toISOString();
    next.cookieHeader = cookieHeader();
    if (!next.serviceRoot || !next.dsid) throw new Error('FIND_MY_UNAVAILABLE');
    return next;
  });
}

function requestTrustedDeviceCode(session) {
  if (!session || !session.sessionId || !session.scnt) return Promise.reject(new Error('NO_2FA_CHALLENGE'));
  restoreCookies(session.cookieHeader);
  return requestJson('PUT', AUTH_ROOT + '/verify/trusteddevice/securitycode', {},
    authHeaders(session.clientId, session)).then(function(response) {
    if (response.status !== 200 && response.status !== 202 && response.status !== 204) {
      throw new Error('SEND_2FA_' + appleError(response));
    }
    var next = withHeaderSession(session, response.headers);
    next.needs2FA = true;
    return next;
  });
}

function startLogin(appleId, password, previousSession) {
  var cleanAppleId = String(appleId || '').trim();
  if (!cleanAppleId || !password) return Promise.reject(new Error('CREDENTIALS_REQUIRED'));
  var clientId = previousSession && previousSession.clientId || randomClientId();
  var session = { appleId: cleanAppleId, clientId: clientId, createdAt: new Date().toISOString() };
  var headers = authHeaders(clientId);
  var srpClient;
  cookieJar = {};

  return requestJson('GET', AUTH_ROOT + '/authorize/signin', {
    frame_id: clientId, skVersion: '7', iframeid: clientId, client_id: WIDGET_KEY,
    response_type: 'code', redirect_uri: 'https://www.icloud.com',
    response_mode: 'web_message', state: clientId, authVersion: 'latest'
  }, headers).then(function(authorizeResponse) {
    if (authorizeResponse.status !== 200) throw new Error('AUTH_BOOTSTRAP_' + appleError(authorizeResponse));
    session = withHeaderSession(session, authorizeResponse.headers);
    headers = authHeaders(clientId, session);
    srpClient = srp.createSrpClient(srpEntropy(authorizeResponse));
    return requestJson('POST', AUTH_ROOT + '/signin/init', {}, headers, {
      a: srpClient.publicA,
      accountName: cleanAppleId,
      protocols: ['s2k', 's2k_fo']
    });
  }).then(function(initResponse) {
    if (initResponse.status !== 200 || !initResponse.body) throw new Error('SRP_INIT_' + appleError(initResponse));
    session = withHeaderSession(session, initResponse.headers);
    headers = authHeaders(clientId, session);
    var proof = srp.createSrpProof(cleanAppleId, password, initResponse.body, srpClient.privateBytes);
    return requestJson('POST', AUTH_ROOT + '/signin/complete', { isRememberMeEnabled: 'true' }, headers, {
      accountName: cleanAppleId,
      c: initResponse.body.c,
      m1: proof.m1,
      m2: proof.m2,
      rememberMe: true,
      trustTokens: previousSession && previousSession.trustToken ? [previousSession.trustToken] : []
    });
  }).then(function(completeResponse) {
    session = withHeaderSession(session, completeResponse.headers);
    if (completeResponse.status === 409) {
      session.needs2FA = true;
      return requestTrustedDeviceCode(session).then(function(nextSession) {
        return { needs2FA: true, session: nextSession };
      });
    }
    if (completeResponse.status !== 200) throw new Error('SIGN_IN_' + appleError(completeResponse));
    return accountLogin(session).then(function(readySession) {
      return { needs2FA: false, session: readySession };
    });
  });
}

function verifyTwoFactor(session, code) {
  var verifiedSession = session;
  if (!session || !session.sessionId || !session.scnt) return Promise.reject(new Error('NO_2FA_CHALLENGE'));
  restoreCookies(session.cookieHeader);
  return requestJson('POST', AUTH_ROOT + '/verify/trusteddevice/securitycode', {}, authHeaders(session.clientId, session), {
    securityCode: { code: String(code || '').replace(/\D/g, '') }
  }).then(function(response) {
    var acceptedConflict = response.status === 409 && Boolean(response.headers['x-apple-session-token']);
    if (response.status !== 200 && response.status !== 204 && !acceptedConflict) {
      throw new Error('INVALID_2FA_' + appleError(response));
    }
    verifiedSession = withHeaderSession(session, response.headers);
    return requestJson('GET', AUTH_ROOT + '/2sv/trust', {}, authHeaders(verifiedSession.clientId, verifiedSession));
  }).then(function(response) {
    if (response.status !== 200 && response.status !== 204) throw new Error('TRUST_2FA_' + appleError(response));
    var trusted = withHeaderSession(verifiedSession, response.headers);
    trusted.needs2FA = false;
    return accountLogin(trusted);
  });
}

function fmipUrl(session, endpoint) {
  return session.serviceRoot + '/fmipservice/client/web/' + endpoint;
}

function fmipHeaders() {
  return {
    Accept: 'application/json, text/javascript, */*; q=0.01',
    'Content-Type': 'application/json',
    Origin: 'https://www.icloud.com',
    Referer: 'https://www.icloud.com/'
  };
}

function fmipBody() {
  return {
    clientContext: {
      appName: 'iCloud Find (Web)', appVersion: '2.0', timezone: 'Asia/Dubai',
      inactiveTime: 1, apiVersion: '3.0', fmly: true
    }
  };
}

function refreshDevices(session, didBootstrap) {
  restoreCookies(session.cookieHeader);
  var params = Object.assign({}, FMIP_PARAMS, { clientId: session.clientId, dsid: session.dsid });
  return requestJson('POST', fmipUrl(session, 'initClient'), params, fmipHeaders(), fmipBody()).then(function(response) {
    if (response.status === 401 || response.status === 403 || response.status === 421) throw new Error('AUTH_EXPIRED');
    if (response.status === 450) {
      if (didBootstrap) throw new Error('REFRESH_HTTP_450');
      return accountLogin(session).then(function(next) { return refreshDevices(next, true); });
    }
    if (response.status !== 200 || !response.body) throw new Error('REFRESH_HTTP_' + response.status);
    var devices = (response.body.content || []).filter(function(device) {
      return String(device.deviceClass || '').toLowerCase() === 'iphone' && device.features && device.features.SND;
    }).map(function(device) {
      return { id: String(device.id || ''), name: String(device.name || device.deviceDisplayName || 'iPhone') };
    });
    var updated = Object.assign({}, session, { cookieHeader: cookieHeader(), updatedAt: new Date().toISOString() });
    return { session: updated, devices: devices };
  });
}

function playSound(session, deviceId) {
  restoreCookies(session.cookieHeader);
  var activeSession = session;
  function send(activeSession) {
    var params = Object.assign({}, FMIP_PARAMS, { clientId: activeSession.clientId, dsid: activeSession.dsid });
    return requestJson('POST', fmipUrl(activeSession, 'playSound'), params, fmipHeaders(), {
      device: deviceId,
      subject: 'Find My iPhone from Pebble',
      clientContext: { fmly: true }
    });
  }
  return send(activeSession).then(function(response) {
    if (response.status === 450) return accountLogin(activeSession).then(function(nextSession) {
      activeSession = nextSession;
      return send(activeSession);
    });
    return response;
  }).then(function(response) {
    if (response.status === 401 || response.status === 403 || response.status === 421) throw new Error('AUTH_EXPIRED');
    if (response.status === 429) throw new Error('RATE_LIMITED');
    if (response.status !== 200) throw new Error('PLAY_HTTP_' + response.status);
    var first = response.body && response.body.content && response.body.content[0] || {};
    var statusCode = String(first.snd && first.snd.statusCode || first.msg && first.msg.statusCode || '');
    if (statusCode !== '200') throw new Error('PLAY_STATUS_' + (statusCode || 'UNKNOWN'));
    return { accepted: true, session: Object.assign({}, activeSession, { cookieHeader: cookieHeader(), updatedAt: new Date().toISOString() }) };
  });
}

module.exports = {
  accountLogin: accountLogin,
  playSound: playSound,
  refreshDevices: refreshDevices,
  requestTrustedDeviceCode: requestTrustedDeviceCode,
  requestJson: requestJson,
  startLogin: startLogin,
  verifyTwoFactor: verifyTwoFactor,
  _restoreCookies: restoreCookies
};

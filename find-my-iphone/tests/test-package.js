'use strict';

var assert = require('assert');
var fs = require('fs');
var path = require('path');

var project = path.join(__dirname, '..');
var pkg = require(path.join(project, 'package.json'));
var cSource = fs.readFileSync(path.join(project, 'src', 'c', 'main.c'), 'utf8');
var pkjsSource = fs.readFileSync(path.join(project, 'src', 'pkjs', 'index.js'), 'utf8');
var assetBuilder = fs.readFileSync(path.join(project, 'tools', 'build-assets.swift'), 'utf8');

assert.strictEqual(pkg.pebble.displayName, 'Find My iPhone');
assert.deepStrictEqual(pkg.pebble.targetPlatforms, ['emery']);
assert.strictEqual(pkg.pebble.watchapp.watchface, false);
assert(pkg.pebble.capabilities.indexOf('configurable') !== -1);
assert.strictEqual(pkg.pebble.capabilities.indexOf('location'), -1);

[
  'COMMAND', 'REQUEST_ID', 'SELECTED_DEVICE', 'STATE', 'RESULT', 'ERROR_CODE',
  'DEVICE_COUNT', 'DEVICE_NAME', 'DEVICE_INDEX', 'LOCALE'
].forEach(function(key) {
  assert(pkg.pebble.messageKeys.indexOf(key) !== -1, 'missing AppMessage key ' + key);
});

pkg.pebble.resources.media.forEach(function(resource) {
  var resourcePath = path.join(project, 'resources', resource.file);
  assert(fs.existsSync(resourcePath), 'missing resource ' + resource.file);
  assert(fs.statSync(resourcePath).size > 0, 'empty resource ' + resource.file);
});

assert(cSource.indexOf('#define ARM_DELAY_MS 650') !== -1, 'watch must require a 650 ms hold');
assert(cSource.indexOf('#define APP_MESSAGE_ACK_TIMEOUT_MS 3000') !== -1,
  'watch must detect a missing AppMessage ACK after three seconds');
assert(cSource.indexOf('#define RING_FRAME_MS 150') !== -1, 'ringing animation must stay calm and readable');
assert(cSource.indexOf('s_ring_phase = (s_ring_phase + 1) % 4') !== -1, 'ringing animation must use four wave phases');
assert(cSource.indexOf('GRect(24, 67, 145, 99)') !== -1, 'foreground canvas must preserve transparent safety padding');
assert(assetBuilder.indexOf('width: 145, height: 99, padding: 7') !== -1,
  'foreground bitmap must match its native layer and retain transparent padding');
assert(cSource.indexOf('GRect(140, 111, 60, 64)') !== -1, 'ringing waves must originate at the iPhone head');
assert(assetBuilder.indexOf('foreground-v4-imagegen.png') !== -1, 'foreground must be rebuilt from the approved ImageGen source');
assert(assetBuilder.indexOf('background-v5-imagegen.png') !== -1, 'background must be rebuilt from the highlighted ImageGen source');
assert(cSource.indexOf('window_raw_click_subscribe') !== -1, 'watch must distinguish press and release');
assert(cSource.indexOf('window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler)') !== -1,
  'Back must recover from ERROR before closing the app');
assert(cSource.indexOf('app_message_register_outbox_sent(outbox_sent_handler)') !== -1,
  'AppMessage ACK handling must be registered');
assert(cSource.indexOf('MESSAGE_KEY_DEVICE_INDEX') !== -1, 'device switching must be explicit');
assert(cSource.indexOf('i18n_get_system_locale()') !== -1, 'watch locale must come from Pebble OS');
assert(pkjsSource.indexOf("writeJson(SESSION_KEY, session)") !== -1, 'session persistence is missing');
assert(pkjsSource.indexOf('response.password = \'\'') !== -1, 'transient password must be cleared');
assert(pkjsSource.indexOf('response.code = \'\'') !== -1, 'transient 2FA code must be cleared');
assert(pkjsSource.indexOf('preferences.lastAcceptedRequestId') !== -1,
  'accepted playSound request IDs must survive duplicate AppMessages');

console.log('package, resources and native contract tests passed');

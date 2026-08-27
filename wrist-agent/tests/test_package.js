'use strict';

var assert = require('assert');
var fs = require('fs');
var path = require('path');

var project = path.join(__dirname, '..');
var pkg = require(path.join(project, 'package.json'));
var config = require(path.join(project, 'src', 'pkjs', 'config.js'));
var mainSource = fs.readFileSync(path.join(project, 'src', 'c', 'main.c'), 'utf8');

assert.strictEqual(pkg.pebble.displayName, 'Wrist Agent');
assert.strictEqual(pkg.pebble.watchapp.watchface, false);
assert.deepStrictEqual(pkg.pebble.targetPlatforms, ['emery']);
assert(pkg.pebble.capabilities.indexOf('configurable') !== -1);
assert.strictEqual(pkg.pebble.capabilities.indexOf('microphone'), -1);

[
  'TRANSCRIPT',
  'COMMAND_NONCE',
  'REFRESH_REQUEST',
  'CONFIRM_REQUEST',
  'AGENT_STATE',
  'RESULT_TEXT',
  'ACTION_SUMMARY',
  'ERROR_CODE',
  'CAN_CONFIRM',
  'RETRY_SAME_REQUEST',
  'VIBRATE_ON_RESULT'
].forEach(function(key) {
  assert(pkg.pebble.messageKeys.indexOf(key) !== -1, 'missing message key ' + key);
});

var configuredKeys = [];
function visit(items) {
  items.forEach(function(item) {
    if (item.messageKey) {
      configuredKeys.push(item.messageKey);
    }
    if (item.items) {
      visit(item.items);
    }
  });
}
visit(config);
['BRIDGE_URL', 'DEVICE_TOKEN', 'POLL_SECONDS', 'VIBRATE_ON_RESULT'].forEach(function(key) {
  assert(configuredKeys.indexOf(key) !== -1, 'missing Clay setting ' + key);
});

assert(mainSource.indexOf('status != DictationSessionStatusSuccess') !== -1,
  'dictation start must compare the success enum explicitly');
assert(mainSource.indexOf('app_message_inbox_size_maximum()') !== -1,
  'AppMessage must allocate the runtime maximum for long UTF-8 answers');
assert(mainSource.indexOf('MESSAGE_KEY_CONFIRM_REQUEST') !== -1,
  'watch action approval must use a separate message');

var icon = path.join(project, 'resources', 'images', 'wrist-agent-menu-icon.png');
assert(fs.existsSync(icon), 'menu icon is missing');
assert(fs.statSync(icon).size > 0, 'menu icon is empty');

console.log('package/config/native contract tests passed');

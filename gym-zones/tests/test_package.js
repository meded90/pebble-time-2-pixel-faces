'use strict';

var assert = require('assert');
var fs = require('fs');
var path = require('path');

var project = path.join(__dirname, '..');
var pkg = require(path.join(project, 'package.json'));
var config = require(path.join(project, 'src', 'pkjs', 'config.js'));

assert.strictEqual(pkg.pebble.displayName, 'Gym Zones');
assert.strictEqual(pkg.pebble.watchapp.watchface, false);
assert.deepStrictEqual(pkg.pebble.targetPlatforms, ['emery']);
assert(pkg.pebble.capabilities.indexOf('health') !== -1);
assert(pkg.pebble.capabilities.indexOf('configurable') !== -1);

var configuredKeys = [];
var restOptions = [];

function visit(items) {
  items.forEach(function(item) {
    if (item.messageKey) {
      configuredKeys.push(item.messageKey);
    }
    if (['ZONE_MODE', 'AGE_FORMULA', 'TARGET_ZONE', 'REST_PRESET']
      .indexOf(item.messageKey) !== -1) {
      assert.strictEqual(item.serializeValueAs, 'integer',
        item.messageKey + ' must be sent to native C as an integer');
    }
    if (item.messageKey === 'REST_PRESET') {
      restOptions = item.options.map(function(option) {
        return Number(option.value);
      });
    }
    if (item.items) {
      visit(item.items);
    }
  });
}

visit(config);
pkg.pebble.messageKeys.forEach(function(messageKey) {
  assert(configuredKeys.indexOf(messageKey) !== -1,
    'Clay config is missing ' + messageKey);
});
assert.deepStrictEqual(restOptions, [90, 120, 180, 300]);

var icon = path.join(project, 'resources', 'images', 'gym-zones-menu-icon.png');
assert(fs.existsSync(icon), 'menu icon is missing');
assert(fs.statSync(icon).size > 0, 'menu icon is empty');

console.log('package/config tests passed');

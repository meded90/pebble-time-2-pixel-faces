module.exports = function() {
  var clayConfig = this;
  var applyingPreset = false;
  var palettes = {
    classic: {
      background: 0x555555,
      line: 0x000000,
      digit: 0xFFFFAA
    },
    midnight: {
      background: 0x000055,
      line: 0xFF5555,
      digit: 0xFFFFAA
    },
    forest: {
      background: 0x005500,
      line: 0x000000,
      digit: 0xFFFFAA
    },
    burgundy: {
      background: 0x550000,
      line: 0x000000,
      digit: 0xFFFFAA
    },
    paper: {
      background: 0xFFFFFF,
      line: 0x000000,
      digit: 0x555555
    },
    amber: {
      background: 0x000000,
      line: 0xAA5500,
      digit: 0xFFAA00
    }
  };

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var preset = clayConfig.getItemById('color_preset');
    var background = clayConfig.getItemByMessageKey('BACKGROUND_COLOR');
    var line = clayConfig.getItemByMessageKey('LINE_COLOR');
    var digit = clayConfig.getItemByMessageKey('DIGIT_COLOR');

    function applySelectedPreset() {
      var palette = palettes[preset.get()];
      if (!palette) {
        return;
      }

      applyingPreset = true;
      background.set(palette.background);
      line.set(palette.line);
      digit.set(palette.digit);
      applyingPreset = false;
    }

    function markAsCustom() {
      if (!applyingPreset) {
        preset.set('custom');
      }
    }

    preset.on('change', applySelectedPreset);
    background.on('change', markAsCustom);
    line.on('change', markAsCustom);
    digit.on('change', markAsCustom);
  });
};

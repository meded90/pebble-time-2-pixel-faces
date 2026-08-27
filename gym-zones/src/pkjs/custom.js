module.exports = function() {
  var clayConfig = this;

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var mode = clayConfig.getItemById('zone_mode');

    function setGroupVisible(groupName, visible) {
      clayConfig.getItemsByGroup(groupName).forEach(function(item) {
        if (visible) {
          item.show();
        } else {
          item.hide();
        }
      });
    }

    function updateZoneFields() {
      var selected = String(mode.get());
      setGroupVisible('max_hr', selected === '1');
      setGroupVisible('age', selected === '2');
      setGroupVisible('manual', selected === '3');
    }

    mode.on('change', updateZoneFields);
    updateZoneFields();
  });
};

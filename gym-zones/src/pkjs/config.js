module.exports = [
  {
    type: 'heading',
    defaultValue: 'Gym Zones'
  },
  {
    type: 'text',
    defaultValue: 'Configure heart-rate zones and the default rest interval. Settings stay locally in Clay and on your watch; Gym Zones has no server. HRV is a PPG estimate, not medical advice.'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Heart-rate zones'
      },
      {
        type: 'select',
        id: 'zone_mode',
        messageKey: 'ZONE_MODE',
        serializeValueAs: 'integer',
        defaultValue: '1',
        label: 'Zone calculation',
        options: [
          { label: 'Manual maximum HR', value: '1' },
          { label: 'Age formula', value: '2' },
          { label: 'Manual zone limits', value: '3' }
        ]
      },
      {
        type: 'slider',
        group: 'max_hr',
        messageKey: 'MAX_HR',
        defaultValue: 180,
        label: 'Maximum HR',
        description: 'Allowed range: 100–240 BPM.',
        min: 100,
        max: 240,
        step: 1
      },
      {
        type: 'slider',
        group: 'age',
        messageKey: 'AGE',
        defaultValue: 30,
        label: 'Age',
        min: 14,
        max: 100,
        step: 1
      },
      {
        type: 'select',
        group: 'age',
        messageKey: 'AGE_FORMULA',
        serializeValueAs: 'integer',
        defaultValue: '1',
        label: 'Maximum HR formula',
        options: [
          { label: '208 − 0.7 × age', value: '1' },
          { label: '220 − age', value: '2' }
        ]
      },
      {
        type: 'text',
        group: 'manual',
        defaultValue: 'Enter five strictly increasing lower limits. Values below Z1 are shown as recovery.'
      },
      {
        type: 'slider',
        group: 'manual',
        messageKey: 'ZONE_1_MIN',
        defaultValue: 90,
        label: 'Z1 from',
        min: 40,
        max: 220,
        step: 1
      },
      {
        type: 'slider',
        group: 'manual',
        messageKey: 'ZONE_2_MIN',
        defaultValue: 108,
        label: 'Z2 from',
        min: 40,
        max: 225,
        step: 1
      },
      {
        type: 'slider',
        group: 'manual',
        messageKey: 'ZONE_3_MIN',
        defaultValue: 126,
        label: 'Z3 from',
        min: 40,
        max: 230,
        step: 1
      },
      {
        type: 'slider',
        group: 'manual',
        messageKey: 'ZONE_4_MIN',
        defaultValue: 144,
        label: 'Z4 from',
        min: 40,
        max: 235,
        step: 1
      },
      {
        type: 'slider',
        group: 'manual',
        messageKey: 'ZONE_5_MIN',
        defaultValue: 162,
        label: 'Z5 from',
        min: 40,
        max: 240,
        step: 1
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Target and vibration'
      },
      {
        type: 'select',
        messageKey: 'TARGET_ZONE',
        serializeValueAs: 'integer',
        defaultValue: '0',
        label: 'Target zone',
        options: [
          { label: 'Off', value: '0' },
          { label: 'Z1', value: '1' },
          { label: 'Z2', value: '2' },
          { label: 'Z3', value: '3' },
          { label: 'Z4', value: '4' },
          { label: 'Z5', value: '5' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'ZONE_VIBES',
        defaultValue: false,
        label: 'Target-zone alerts',
        description: 'Short pulse on entry, double pulse on exit. Suppressed during rest.'
      },
      {
        type: 'toggle',
        messageKey: 'VIBRATIONS_ENABLED',
        defaultValue: true,
        label: 'Vibration',
        description: 'Master switch for zone and rest alerts.'
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Rest timer'
      },
      {
        type: 'select',
        messageKey: 'REST_PRESET',
        serializeValueAs: 'integer',
        defaultValue: '120',
        label: 'Default rest',
        options: [
          { label: '1:30', value: '90' },
          { label: '2:00', value: '120' },
          { label: '3:00', value: '180' },
          { label: '5:00', value: '300' }
        ]
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save settings'
  },
  {
    type: 'footer',
    defaultValue: 'Gym Zones · Pebble Time 2 · HRV is not a readiness score.'
  }
];

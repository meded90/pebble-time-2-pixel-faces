module.exports = [
  {
    type: 'heading',
    defaultValue: 'Flip Board'
  },
  {
    type: 'text',
    defaultValue: 'Choose the colors used by the watchface.'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Color preset'
      },
      {
        type: 'select',
        id: 'color_preset',
        defaultValue: 'custom',
        label: 'Preset',
        description: 'Choose a palette, then fine-tune any color below.',
        options: [
          {
            label: 'Custom',
            value: 'custom'
          },
          {
            label: 'Classic Gray',
            value: 'classic'
          },
          {
            label: 'Midnight Copper',
            value: 'midnight'
          },
          {
            label: 'Forest Brass',
            value: 'forest'
          },
          {
            label: 'Burgundy Cream',
            value: 'burgundy'
          },
          {
            label: 'Paper',
            value: 'paper'
          },
          {
            label: 'Amber Night',
            value: 'amber'
          }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Custom colors'
      },
      {
        type: 'color',
        messageKey: 'BACKGROUND_COLOR',
        defaultValue: '0x555555',
        label: 'Background'
      },
      {
        type: 'color',
        messageKey: 'LINE_COLOR',
        defaultValue: '0x000000',
        label: 'Lines and hinges'
      },
      {
        type: 'color',
        messageKey: 'DIGIT_COLOR',
        defaultValue: '0xE8E0C8',
        label: 'Digits and date'
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Save colors'
  }
];

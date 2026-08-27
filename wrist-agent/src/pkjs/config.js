'use strict';

module.exports = [
  {
    type: 'heading',
    defaultValue: 'Wrist Agent'
  },
  {
    type: 'text',
    defaultValue: 'Wrist Agent sends accepted dictation text to your own HTTPS bridge. The bridge, not the watch, stores the ChatGPT Workspace Agent credential.'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Private bridge'
      },
      {
        type: 'input',
        messageKey: 'BRIDGE_URL',
        defaultValue: '',
        label: 'Bridge URL',
        description: 'Base HTTPS URL of your deployed wrist-agent-server.',
        attributes: {
          type: 'url',
          placeholder: 'https://agent.example.com'
        }
      },
      {
        type: 'input',
        messageKey: 'DEVICE_TOKEN',
        defaultValue: '',
        label: 'Device token',
        description: 'A random token from WRIST_AGENT_DEVICE_TOKENS. This is not an OpenAI token.',
        attributes: {
          type: 'password',
          placeholder: 'Paste the bridge device token'
        }
      },
      {
        type: 'select',
        messageKey: 'POLL_SECONDS',
        defaultValue: '4',
        label: 'Result refresh',
        options: [
          { label: 'Every 3 seconds', value: '3' },
          { label: 'Every 4 seconds', value: '4' },
          { label: 'Every 6 seconds', value: '6' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'VIBRATE_ON_RESULT',
        defaultValue: true,
        label: 'Vibrate on result'
      }
    ]
  },
  {
    type: 'text',
    defaultValue: 'Privacy: accepted speech text is sent through your phone to your bridge and ChatGPT workspace. Keep both tokens secret. The app contains no developer-operated analytics or service.'
  },
  {
    type: 'submit',
    defaultValue: 'Save connection'
  },
  {
    type: 'footer',
    defaultValue: 'Setup guide: github.com/meded90/pebble-time-2-pixel-faces/tree/main/wrist-agent'
  }
];

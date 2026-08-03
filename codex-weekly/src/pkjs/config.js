module.exports = [
  {
    type: 'heading',
    defaultValue: 'Codex Weekly'
  },
  {
    type: 'text',
    defaultValue: 'Sign in on your Mac with <code>codex login</code>, then connect this watchface to the local Codex bridge. <a href="https://learn.chatgpt.com/docs/auth.md">Authentication guide</a>. Your ChatGPT credentials never go to the watch.'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Codex bridge'
      },
      {
        type: 'input',
        messageKey: 'BRIDGE_URL',
        defaultValue: '',
        label: 'Status URL',
        description: 'Example: http://192.168.1.20:8765/status',
        attributes: {
          type: 'url',
          placeholder: 'http://192.168.1.20:8765/status'
        }
      },
      {
        type: 'input',
        messageKey: 'BRIDGE_TOKEN',
        defaultValue: '',
        label: 'Bridge token',
        description: 'Generate it locally with: openssl rand -hex 24. Use the CODEX_PEBBLE_TOKEN value, not an OpenAI API key.',
        attributes: {
          type: 'password',
          placeholder: 'Paste the bridge token'
        }
      },
      {
        type: 'select',
        messageKey: 'REFRESH_MINUTES',
        defaultValue: '30',
        label: 'Refresh interval',
        options: [
          {
            label: '15 minutes',
            value: '15'
          },
          {
            label: '30 minutes',
            value: '30'
          },
          {
            label: '60 minutes',
            value: '60'
          }
        ]
      }
    ]
  },
  {
    type: 'text',
    defaultValue: 'The bridge uses the existing ChatGPT login in Codex on your Mac. OpenAI Platform API keys cannot read personal Codex limits. Do not expose the bridge to the public internet without HTTPS and authentication.'
  },
  {
    type: 'submit',
    defaultValue: 'Save and sync'
  }
];

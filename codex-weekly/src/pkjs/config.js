module.exports = [
  {
    type: 'heading',
    defaultValue: 'Codex Weekly'
  },
  {
    type: 'text',
    defaultValue: 'Deploy your own Cloud Run bridge, then paste its status URL and client token below.'
  },
  {
    type: 'text',
    defaultValue: 'Server deployment guides: <a href="https://github.com/meded90/pebble-time-2-pixel-faces/blob/master/codex-weekly/cloud-run/README.md" target="_blank" rel="noopener">Русский</a> · <a href="https://github.com/meded90/pebble-time-2-pixel-faces/blob/master/codex-weekly/cloud-run/README.en.md" target="_blank" rel="noopener">English</a>.'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Codex Weekly connection'
      },
      {
        type: 'input',
        messageKey: 'BRIDGE_URL',
        defaultValue: '',
        label: 'Status URL',
        description: 'Your own Cloud Run service URL ending in /status.',
        attributes: {
          type: 'url',
          placeholder: 'https://YOUR-SERVICE-URL.run.app/status'
        }
      },
      {
        type: 'input',
        messageKey: 'BRIDGE_TOKEN',
        defaultValue: '',
        label: 'Cloud Run client token',
        description: 'Retrieve it from your own codex-weekly-client-token secret. This is not an OpenAI API key.',
        attributes: {
          type: 'password',
          placeholder: 'Paste your Secret Manager token'
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
      },
      {
        type: 'button',
        id: 'check_server_status',
        primary: true,
        defaultValue: 'Check server status',
        description: 'Tests the URL and client token entered above and reports how many Personal Usage days were received, without saving settings.'
      },
      {
        type: 'text',
        id: 'server_status_result',
        defaultValue: 'Status check has not run yet.'
      }
    ]
  },
  {
    type: 'text',
    defaultValue: 'The token stays in PebbleKit settings on your phone and is sent over HTTPS. Never publish it in chats, URLs, or Git. Sync failures show neutral missing-data indicators.'
  },
  {
    type: 'submit',
    defaultValue: 'Save and sync'
  }
];

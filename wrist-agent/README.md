# Wrist Agent

**English** · [Русский](README.ru.md) · [All projects](../README.md#project-gallery)

Voice front end for a ChatGPT Workspace Agent on Pebble Time 2. The watch opens
system Dictation, lets the user review the accepted transcription once more,
sends only that text through the phone to a private bridge, and displays a
compact answer or action outcome.

## Requirements

- Pebble Time 2 (`emery`) with a compatible Pebble/Core Devices phone app and
  working voice Dictation;
- Bluetooth and phone internet access;
- your own HTTPS deployment of [`wrist-agent-server`](../wrist-agent-server/);
- a published Workspace Agent API channel and server-side Workspace Agent
  access token;
- the bridge's private `/mcp` connection attached to the agent.

The OpenAI credential never enters the PBW, watch, or Clay settings. Configure
only the bridge base URL and its separate device token in the phone app.

## Controls

- Select on Ready starts Dictation.
- Accept the system transcription, then review the exact command on the watch.
- Select sends; Back cancels.
- Up and Down scroll long commands and results.
- Select refreshes an active run.
- When the agent proposes a consequential write, Select approves it in a
  separate idempotent run; Back rejects it.
- Select on Done starts a new request.

## Privacy

The configured Pebble/Core Devices dictation service processes audio; Wrist
Agent receives only the accepted transcription. PebbleKit JS keeps connection
settings and minimal pending-request state. Your bridge sends the command to
your ChatGPT workspace and keeps compact status/results for 24 hours by
default. The app has no developer-operated service, advertising, telemetry, or
analytics. Read the server's [`SECURITY.md`](../wrist-agent-server/SECURITY.md).

## Build and test

```bash
cd wrist-agent
npm ci
npm test
../.venv/bin/pebble clean
../.venv/bin/pebble build
../.venv/bin/pebble install --emulator emery
../.venv/bin/pebble transcribe --emulator emery "Create a test reminder"
```

Package: `build/wrist-agent.pbw`. Follow [`PUBLISHING.md`](PUBLISHING.md) for
real-device, bridge, MCP, and marketplace checks.

The Workspace Agents trigger API currently exposes run status but not response
text, so the `send_to_pebble` MCP callback is required. See the official
[trigger-runs documentation](https://developers.openai.com/workspace-agents/trigger-runs).

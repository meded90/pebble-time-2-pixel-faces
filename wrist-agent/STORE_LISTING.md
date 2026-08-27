# RePebble listing draft

## Name

Wrist Agent

## Short description

Speak to your private ChatGPT Workspace Agent and receive concise answers on Pebble Time 2.

## Description

Wrist Agent turns Pebble Time 2 into a compact voice remote for a ChatGPT
Workspace Agent you control. Press Select, use system Dictation, review the
recognized command, and send it through your phone to your own HTTPS bridge.
The result and any verified action summary return to the watch in a scrollable,
high-contrast interface.

Your Workspace Agent can use only the apps, connectors, web access, and MCP
tools enabled by you or your workspace administrator. Consequential write
actions can be proposed first and approved with a separate Select press on the
watch. If ChatGPT itself requires attention, Wrist Agent tells you to open the
conversation on your phone.

Wrist Agent is self-hosted infrastructure, not a bundled ChatGPT account. It
requires the included open-source bridge, a published Workspace Agent API
channel, and a private MCP callback connection. Setup instructions are provided
at the support website.

## Compatibility

- Pebble Time 2 (`emery`) only
- PebbleOS 4.32 or newer
- Compatible Pebble/Core Devices phone app with working Dictation
- Connected phone and internet
- Self-hosted Wrist Agent bridge and eligible ChatGPT workspace

## Privacy

The configured Pebble/Core Devices dictation service processes voice audio.
The watchapp receives only the transcription accepted by the user and sends it
through the connected phone to the user's own bridge and ChatGPT workspace.
The phone stores the bridge URL, a separate device token, and minimal current
request state. The default bridge retains compact request results for 24 hours.

The app has no developer-operated service, advertising, analytics, or
telemetry. Workspace/OpenAI credentials stay on the user's server and are never
stored on the watch or in phone settings.

## Permissions

- Configurable phone settings
- Internet access through PebbleKit JS
- System Dictation through the connected phone

## Support and source

https://github.com/meded90/pebble-time-2-pixel-faces/tree/main/wrist-agent

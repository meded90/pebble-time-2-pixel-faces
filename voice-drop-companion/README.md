# Voice Drop mobile bridge

**English** · [Русский](README.ru.md)

Voice Drop intentionally does not use a separate PebbleKit application because
it cannot receive raw system Pebble Protocol endpoint packets. Instead,
`coredevices-mobileapp.patch` adds a shared Kotlin Multiplatform module directly
to the open-source Core Devices mobile app for iOS and Android.

The module:

- listens on endpoint `10001`;
- starts a pull on connection and on the watch's 30-minute signal;
- acknowledges each block by offset;
- assembles at most 8 MB and verifies CRC32 and Speex frame count;
- calls only an explicitly installed `VoiceDropUploadSink`;
- acknowledges deletion only after `upload(...) == true`.

The default sink is `null`, so audio stays on the watch. The host app must set
`VoiceDropUploadSinkRegistry.sink` only after the user provides an exact HTTPS
URL and separate device token. A Telegram bot token never enters the mobile app.

```bash
git clone https://github.com/coredevices/mobileapp.git
cd mobileapp
git apply /path/to/voice-drop-companion/coredevices-mobileapp.patch
```

`VoiceDropRecording.toContainer()` produces the payload accepted by
[`voice-drop-server`](../voice-drop-server/) as `application/x-voice-drop`.

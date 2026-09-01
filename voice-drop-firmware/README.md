# PebbleOS Voice Drop patch

**English** · [Русский](README.ru.md)

This patch targets the current Core Devices PebbleOS `main` branch and Emery /
Pebble Time 2 only. It adds:

- a Speex 16 kHz, 9.8 kbps microphone recording service;
- an 8 MB PFS queue with 16 KB chunks;
- safe final-chunk and metadata commits;
- cleanup of interrupted recordings at boot;
- Pebble Protocol endpoint `10001` with pull/ACK transfer;
- a queue-presence signal every 30 minutes;
- `voice_drop_start`, `voice_drop_stop`, and `voice_drop_get_status` SDK APIs;
- SDK revision 110.

`voice_drop_stop()` stops the microphone, closes every PFS chunk, then writes
the metadata commit last. Until it completes, the API returns
`VoiceDropStateStopping`; the recording is not eligible for synchronization.

```bash
git clone https://github.com/coredevices/PebbleOS.git
cd PebbleOS
git apply /path/to/voice-drop-firmware/pebbleos.patch
python3 ./pbl configure --board obelix@pvt
python3 ./pbl build
```

Use the generated `build/sdk/emery` SDK to build [`voice-drop`](../voice-drop/).
The public SDK does not contain the required shim functions.

The patch was compiled and linked for `qemu_emery`, and native SDK generation
found all three functions. A later full QEMU build stopped in the stock demo
application because of the installed Homebrew ARM toolchain's `stdint.h`
compatibility, not in Voice Drop code.

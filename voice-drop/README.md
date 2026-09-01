# Voice Drop for Pebble Time 2

**English** · [Русский](README.ru.md)

A minimal standalone voice recorder. It does not use the phone microphone:
audio is encoded as Speex on the watch and stored in PebbleOS internal storage.
After recording stops, the file remains on the watch until delivery is
explicitly acknowledged.

The interface shows the current time, queue size, recording timer, and state.
Press Select to start and stop recording. The app enters `SAVING` while the
firmware flushes the final audio chunk and writes the recording metadata last.
Closing the app during recording uses the same safe finalization path.

## Requirements

Public PebbleOS and the public SDK do not expose the microphone to watchapps and
limit persistent app storage to 4 KB. Voice Drop 0.2.0 therefore requires the
[`pebbleos.patch`](../voice-drop-firmware/pebbleos.patch) and SDK revision 110
built from it.

The firmware queue is limited to 8 MB, approximately 100–110 minutes at
9.8 kbps. Data is committed in 16 KB chunks. Incomplete chunks are removed
after power loss while completed recordings remain queued.

The shared iOS/Android bridge is provided by
[`coredevices-mobileapp.patch`](../voice-drop-companion/coredevices-mobileapp.patch).
It uses private Pebble Protocol endpoint `10001`, pulls on reconnection and at
30-minute signals, and acknowledges deletion only after an explicitly
configured `VoiceDropUploadSink` succeeds.

The artifact in `../dist/experimental/` is a legacy 0.1.0 phone-recording
prototype. It is not a build of this 0.2.0 source.

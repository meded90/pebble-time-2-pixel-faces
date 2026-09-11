# Starry Digits 1.2.1 validation

Validated on 2026-09-10.

- SDK 4.33.1 builds Emery and Gabbro. Emery resources occupy 257732 of 262144 bytes; Gabbro resources occupy 17553 bytes. The SDK emits its linker RWX warning.
- The generator reverses HTML study 03 and eases geometric displacement to zero. All 46 selected frames preserve fixed moon/city pixels; the final frame equals the original 64-color image exactly.
- The real C delta/LZ decoder matches every generated reference frame under AddressSanitizer and UndefinedBehaviorSanitizer. Checks cover padded framebuffer rows, truncated chunks, invalid backreferences, insufficient scratch capacity and invalid stride.
- Host lifecycle tests cover completion, no replay, cancellation, repeated cleanup, allocation/decode/timer/resource errors, buffer reuse, delayed callbacks and backwards clock changes. Every callback advances one frame rather than selecting an index from wall time.
- Before: 28 of 36 frames rendered; PNG decoding took 329 ms total, 13 ms maximum. A separate trial also observed a backwards wall-clock jump prematurely ending playback.
- After: repeated SDK runs completed all 46 frames and stopped animation timers. Decode totals were 506–545 ms, maximum 13–16 ms; this does not establish a decode speed improvement. The change eliminates intentional frame skipping and adds intermediate poses.
- Final 1.2.1 runtime: 46/46 frames, 522 ms total decode time, 16 ms maximum; final rendered screen verified.
- Runtime logs report 70352 free heap bytes after completion. Playback lasts approximately five seconds, including decode and scheduler overhead. The frame hold is 87 ms; delayed execution may extend total duration.
- Browser-first QEMU/WASM booted Emery but stalled at BlobDB installation. A pre-existing SDK emulator also timed out and was preserved. Validation therefore uses a separate SDK 4.33.1 QEMU with clean flash under `/tmp/starry-emery-clean` and a local noVNC screen.
- Inspected running and final rendered screens with all four digits visible; launcher exit and return starts a fresh introduction. Fixed regions and exact final pixels are also checked programmatically.
- Round 2 is build-verified only. Physical watch performance and battery impact have not been measured.

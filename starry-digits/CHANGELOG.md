# Changelog

## 1.2.1 — 2026-09-10

- Reverse the opening sky motion and ease its displacement into the exact original background as the final frame, without a crossfade.
- Play all 46 frames sequentially; delayed callbacks and watch-clock adjustments no longer skip frames or prematurely end the introduction.
- Reuse one bitmap and bounded decoding buffers during playback. Release animation memory and stop its timer after completion or loss of focus.
- Keep the 64-color palette, fixed moon and city, and static Round 2 background. Playback takes approximately five seconds in the SDK emulator; physical-watch smoothness and battery impact remain unmeasured.

## 1.2.0 — 2026-09-09

- Play a four-second flowing-sky introduction once when Starry Digits opens on Pebble Time 2, followed by a half-second settle to the still image.
- Keep the moon, city and time digits stationary during the introduction, then restore the original static background.
- Stop the introduction when the face loses focus; returning from an overlay does not restart it during the same app launch.
- Release animation buffers and cancel its timer after playback; normal time updates remain once per minute.
- Keep Pebble Round 2 static. Physical-device battery impact has not been measured.

## 1.1.0

- Add a dedicated Pebble Round 2 layout and background alongside Pebble Time 2 support.
- Render the time with custom hand-drawn bitmap digits over the painted night-sky artwork.

# Starry Digits

**English** · [Русский](README.ru.md) · [All projects](../README.md#project-gallery)

[![Starry Digits](../assets/screenshots/starry-digits.png)](../assets/screenshots/starry-digits.png)

[Install from RePebble](https://apps.repebble.com/starry-digits_bacf5a80f08845558f44cf65).

An artistic watchface for Pebble Time 2 (`emery`, 200×228) and Pebble Round 2
(`gabbro`, 260×260): luminous hand-drawn digits over a pixel-art night sky
formed from swirling painted strokes.

## Features

- custom bitmap artwork for all ten digits;
- dedicated background and safe layout for the 260×260 round display;
- vertically stacked hours and minutes;
- system 12/24-hour preference;
- updates once per minute;
- fully offline with no settings or phone connection.

## Build and run

```bash
pebble build
pebble install --emulator emery --logs
pebble install --emulator gabbro --logs
```

Published package: [`../dist/published/starry-digits-1.1.0.pbw`](../dist/published/starry-digits-1.1.0.pbw).

## Permissions

None.

## All watchfaces

[Mosaic Grid](../mosaic-grid/) · [Flip Board](../flip-board/) · [Info Tiles](../info-tiles/) · [Codex Weekly](../codex-weekly/) · [Starry Digits](../starry-digits/) · [meded90](../meded90/) · [Zodiac: Aquarius](../zodiac-aquarius/) · [Zodiac: Gemini](../zodiac-gemini/)

## Opening animation

Pebble Time 2 plays the approved flowing-sky study once per app launch with 46 reversed flow frames that settle geometrically into the exact original image (approximately five seconds in the SDK emulator), then restores the original background. Losing focus stops playback without replaying it on focus return. Temporary frame memory is freed and no animation timer remains; the clock continues updating once per minute. Pebble Round 2 remains static. Battery impact on physical hardware has not been measured.

Rebuild animation resources with `node tools/build-intro.cjs` (Node.js and the repository Python environment with Pillow). The generator reproduces the approved `previews/animated-sky-v3` renderer with a fixed moon and city. Frames use a lossless delta/LZ pack and a reused framebuffer; delayed callbacks never skip poses. The generator writes a raw reference to `build/intro-reference.bin` for the C decoder round-trip check.

Test package: [`../dist/candidates/starry-digits-1.2.1.pbw`](../dist/candidates/starry-digits-1.2.1.pbw).

Run the native lifecycle tests from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Werror -DPBL_PLATFORM_EMERY -Istarry-digits/tests starry-digits/tests/intro-lifecycle.c -o /tmp/starry-intro-test
/tmp/starry-intro-test
node starry-digits/tools/build-intro.cjs
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined starry-digits/tests/codec-roundtrip.c starry-digits/src/c/intro_codec.c -o /tmp/starry-codec-test
/tmp/starry-codec-test starry-digits/resources/data/intro.bin starry-digits/build/intro-reference.bin
```

Validation details: [`tests/VALIDATION.md`](tests/VALIDATION.md).

# Pebble Time 2 Pixel Faces

[Русский](README.md) · [English](README.en.md)

Eight independent native watchfaces for Pebble Time 2 (`emery`, 200×228,
64 colors). Each directory is a standalone Pebble project with its own UUID
and `.pbw` package.

The repository also contains three standalone watchapps: [Gym Zones](gym-zones/)
for strength workouts, heart-rate zones, rest timing, and one-minute PPG-HRV;
[Wrist Agent](wrist-agent/), a voice front end for a ChatGPT Workspace Agent
with a private bridge and MCP callback; and `voice-drop` for microphone
recording. Start with each app's documentation; they are not watchfaces and are
not included in the gallery below.

## Native 200×228 previews

| [Mosaic Grid](mosaic-grid/) | [Flip Board](flip-board/) | [Info Tiles](info-tiles/) | [Codex Weekly](codex-weekly/) |
| --- | --- | --- | --- |
| [![Mosaic Grid](assets/screenshots/mosaic-grid.png)](mosaic-grid/) | [![Flip Board](assets/screenshots/flip-board.png)](flip-board/) | [![Info Tiles](assets/screenshots/info-tiles.png)](info-tiles/) | [![Codex Weekly](assets/screenshots/codex-weekly.png)](codex-weekly/) |

| [Starry Digits](starry-digits/) | [meded90](meded90/) | [Zodiac: Aquarius](zodiac-aquarius/) | [Zodiac: Gemini](zodiac-gemini/) |
| --- | --- | --- | --- |
| [![Starry Digits](assets/screenshots/starry-digits.png)](starry-digits/) | [![meded90](assets/screenshots/meded90.png)](meded90/) | [![Zodiac: Aquarius](assets/screenshots/zodiac-aquarius.png)](zodiac-aquarius/) | [![Zodiac: Gemini](assets/screenshots/zodiac-gemini.png)](zodiac-gemini/) |

## Watchfaces

1. [Mosaic Grid](mosaic-grid/) — large time in a strict modernist color grid.
2. [Flip Board](flip-board/) — a configurable mechanical four-panel clock.
3. [Info Tiles](info-tiles/) — time, date, battery, weather, heart rate, and steps.
4. [Codex Weekly](codex-weekly/) — time, Codex quota, and a personal usage heatmap.
5. [Starry Digits](starry-digits/) — hand-drawn luminous digits over a painted night.
6. [meded90](meded90/) — a pixel portrait with vertically stacked time.
7. [Zodiac: Aquarius](zodiac-aquarius/) — Aquarius artwork with vertical time.
8. [Zodiac: Gemini](zodiac-gemini/) — Gemini artwork with a bold digital clock.

## Build all watchfaces

Install the Pebble CLI and SDK:

```bash
brew install node libpng uv
uv tool install pebble-tool
pebble sdk install latest
```

Build all eight watchfaces:

```bash
make build
```

Every project writes its package to `<project>/build/*.pbw`. Verified release
copies are stored in `dist/`. Build Gym Zones on its own with SDK 4.33.1:

```bash
make gym-zones PEBBLE="$PWD/.venv/bin/pebble"
```

Wrist Agent uses the same public SDK and has a separately tested bridge:

```bash
make wrist-agent PEBBLE="$PWD/.venv/bin/pebble"
npm --prefix wrist-agent-server test
```

The aggregate `make apps` target also builds `voice-drop`, which requires the
different patched SDK described in its README; do not use that aggregate target
as a Gym Zones- or Wrist Agent-only check.

The verified package is `dist/gym-zones.pbw`. See
[`gym-zones/README.en.md`](gym-zones/README.en.md) and
[`gym-zones/VALIDATION.md`](gym-zones/VALIDATION.md) for controls, screenshots,
and the exact hardware-validation boundary.

## Run one watchface in the emulator

```bash
cd mosaic-grid
pebble build
pebble install --emulator emery --logs
```

Use the same commands in any other watchface directory. If the emulator shows
an old build, run `pebble kill`, `pebble wipe`, build again, and reinstall.

Useful emulator commands:

```bash
pebble emu-time-format --emulator emery --format 12h
pebble emu-time-format --emulator emery --format 24h
pebble emu-battery --emulator emery --percent 9
pebble emu-steps --emulator emery 12345
pebble emu-heart-rate --emulator emery 72
pebble screenshot --emulator emery screenshot.png
```

### Browser-based testing alternative

For testing, you can also use
[pebble-qemu-wasm](https://github.com/ericmigi/pebble-qemu-wasm) as an alternative
to the SDK emulator. It runs Pebble in WebAssembly and displays the watch screen
directly in the browser. Try the
[live demo](https://ericmigi.github.io/pebble-qemu-wasm/) or follow the project's
README to run it locally. This is useful for visual and interactive checks,
including through the Browser plugin in Codex: inspect the result, press watch
buttons with the mouse or keyboard, click through menus, and verify UI behavior.
It is an additional testing option, not a replacement for a final test on a watch.

## Install on a watch

```bash
pebble login
cd mosaic-grid
pebble install --cloudpebble
```

Repeat from the directory of the watchface you want to install.

See [MARKETPLACE_RU.md](MARKETPLACE_RU.md) for the publishing workflow and
[STORE_LISTINGS.md](STORE_LISTINGS.md) for prepared listing copy.

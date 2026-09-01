# Pebble Time 2 Pixel Faces

**English** · [Русский](README.ru.md)

A collection of native Pebble Time 2 projects for the `emery` platform
(200×228, 64 colors): eight watchfaces and four watchapps. Every project has
its own UUID, version, source directory, and independently built `.pbw` package.

<!-- projects:catalog:start -->
## Project gallery

### Watchfaces

| [Mosaic Grid](mosaic-grid/) | [Flip Board](flip-board/) | [Info Tiles](info-tiles/) | [Codex Weekly](codex-weekly/) |
| --- | --- | --- | --- |
| [![Mosaic Grid](assets/screenshots/mosaic-grid.png)](mosaic-grid/) | [![Flip Board](assets/screenshots/flip-board.png)](flip-board/) | [![Info Tiles](assets/screenshots/info-tiles.png)](info-tiles/) | [![Codex Weekly](assets/screenshots/codex-weekly.png)](codex-weekly/) |
| [Install from RePebble](https://apps.repebble.com/mosaic-grid_d2ccde5d2187490085b44f8e) | [Install from RePebble](https://apps.repebble.com/flip-board_d9b87c9f10a74d718db82b06) | Release candidate · Not published | [Install from RePebble](https://apps.repebble.com/codex-weekly_27f48d86803e471a83b93dfe) |

| [Starry Digits](starry-digits/) | [meded90](meded90/) | [Zodiac: Aquarius](zodiac-aquarius/) | [Zodiac: Gemini](zodiac-gemini/) |
| --- | --- | --- | --- |
| [![Starry Digits](assets/screenshots/starry-digits.png)](starry-digits/) | [![meded90](assets/screenshots/meded90.png)](meded90/) | [![Zodiac: Aquarius](assets/screenshots/zodiac-aquarius.png)](zodiac-aquarius/) | [![Zodiac: Gemini](assets/screenshots/zodiac-gemini.png)](zodiac-gemini/) |
| [Install from RePebble](https://apps.repebble.com/starry-digits_bacf5a80f08845558f44cf65) | Release candidate · Not published | Release candidate · Not published | [Install from RePebble](https://apps.repebble.com/zodiac-gemini_a1c61f1227144535accbf53c) |

### Watchapps

| [Gym Zones](gym-zones/) | [Wrist Agent](wrist-agent/) | [Find My iPhone](find-my-iphone/) | [Voice Drop](voice-drop/) |
| --- | --- | --- | --- |
| [![Gym Zones](gym-zones/screenshots/z3.png)](gym-zones/) | [![Wrist Agent](assets/screenshots/wrist-agent.png)](wrist-agent/) | [![Find My iPhone](find-my-iphone/assets/emulator-v9-native-fit.png)](find-my-iphone/) | [![Voice Drop](assets/screenshots/voice-drop.png)](voice-drop/) |
| Release candidate · Not published | Release candidate · Not published | [Install from RePebble](https://apps.repebble.com/7ffae9b84d714b9882f7a40f) | Experimental · Not published |

“Not published” means that the repository contains source code or a release candidate, but no verified public Appstore page. It does not mean the project has passed final hardware or service validation.

## What each project does

| Project | Type | Source version | Purpose |
| --- | --- | --- | --- |
| [Mosaic Grid](mosaic-grid/) | Watchface | 1.0.2 | Large time in a modernist color grid. |
| [Flip Board](flip-board/) | Watchface | 1.2.1 | Configurable mechanical four-panel clock. |
| [Info Tiles](info-tiles/) | Watchface | 1.0.0 | Time, weather, battery, steps, and heart rate. |
| [Codex Weekly](codex-weekly/) | Watchface | 1.0.11 | Codex quota and personal usage heatmap. |
| [Starry Digits](starry-digits/) | Watchface | 1.1.0 | Hand-drawn digits over a painted night sky; also supports Round 2. |
| [meded90](meded90/) | Watchface | 1.0.0 | Pixel portrait with vertically stacked time. |
| [Zodiac: Aquarius](zodiac-aquarius/) | Watchface | 1.3.0 | Illustrated Aquarius face with vertical time. |
| [Zodiac: Gemini](zodiac-gemini/) | Watchface | 1.4.0 | Illustrated Gemini face with large digital time. |
| [Gym Zones](gym-zones/) | Watchapp | 1.0.0 | Strength workout, rest, HR zones, and one-minute PPG-HRV. |
| [Wrist Agent](wrist-agent/) | Watchapp | 1.0.0 | Voice front end for a privately deployed ChatGPT Workspace Agent bridge. |
| [Find My iPhone](find-my-iphone/) | Watchapp | 0.1.4 | Experimental direct Apple Find My sound request from the paired iPhone. |
| [Voice Drop](voice-drop/) | Watchapp | 0.2.0 | On-watch microphone recording with patched firmware and phone bridge. |
<!-- projects:catalog:end -->

## Quick start

Install the Pebble CLI and SDK:

```bash
brew install node libpng uv
uv tool install pebble-tool
pebble sdk install latest
```

Build the eight watchfaces:

```bash
make build
```

Build a standard watchapp individually:

```bash
make gym-zones PEBBLE="$PWD/.venv/bin/pebble"
make wrist-agent PEBBLE="$PWD/.venv/bin/pebble"
make find-my-iphone PEBBLE="$PWD/.venv/bin/pebble"
```

`make standard-apps` builds those three applications. `make apps` additionally
attempts Voice Drop, which requires the patched PebbleOS SDK documented in
[`voice-drop/README.md`](voice-drop/README.md).

Audited release copies are kept in [`dist/`](dist/). A package being present
there does not by itself mean that the corresponding app is publicly released.
The artifact in `dist/experimental/voice-drop-prototype-0.1.0.pbw` is not
compatible with the current Voice Drop 0.2.0 source and should not be installed
as that version.

## Show and test an application

The primary path is the
[pebble-qemu-wasm browser emulator](https://ericmigi.github.io/pebble-qemu-wasm/).
Build or select the requested PBW, boot Emery / Pebble Time 2 in the page, then
use **Upload .pbw** or drag the binary onto the watch. When a user asks to show,
open, or run an application for testing, this browser-first workflow is the
default and the interactive result should remain available to the user.

Use the standard Pebble SDK emulator only if browser boot, WebAssembly, PBW
upload, installation, or application execution fails, or when the requested
behavior needs SDK logs or unsupported hardware/service capabilities:

```bash
pebble install --emulator emery --logs path/to/application.pbw
```

From a project directory, `pebble build` followed by
`pebble install --emulator emery --logs` builds and installs the current source.
If that emulator is stale, run `pebble kill` and `pebble wipe` before rebuilding.

See [TESTING.md](TESTING.md) for the complete browser-first procedure, controls,
local `pebble-qemu-wasm` hosting, fallback criteria, and physical-hardware
validation boundaries.

## Published on RePebble

<!-- projects:published:start -->
- [Mosaic Grid](https://apps.repebble.com/mosaic-grid_d2ccde5d2187490085b44f8e) — 1.0.2
- [Flip Board](https://apps.repebble.com/flip-board_d9b87c9f10a74d718db82b06) — 1.2.1
- [Codex Weekly](https://apps.repebble.com/codex-weekly_27f48d86803e471a83b93dfe) — 1.0.7
- [Starry Digits](https://apps.repebble.com/starry-digits_bacf5a80f08845558f44cf65) — 1.1.0
- [Zodiac: Gemini](https://apps.repebble.com/zodiac-gemini_a1c61f1227144535accbf53c) — 1.3.1
- [Find My iPhone](https://apps.repebble.com/7ffae9b84d714b9882f7a40f) — 0.1.4
<!-- projects:published:end -->

See [MARKETPLACE_RU.md](MARKETPLACE_RU.md) for the publishing workflow and
[STORE_LISTINGS.md](STORE_LISTINGS.md) for listing drafts. Public links are
added only after the Appstore page has been verified.

## Repository conventions

- [`README.md`](README.md) is the canonical English overview;
  [`README.ru.md`](README.ru.md) is its Russian translation.
- Each Pebble project is isolated in its own directory and keeps a stable UUID.
- Every project keeps a cumulative `CHANGELOG.md`. A source version cannot pass
  `npm run projects:check` without a non-empty entry for that exact version.
- Generated `build/` directories and dependencies are ignored; selected release
  packages and visual assets are versioned deliberately.
- Project-specific requirements, privacy boundaries, and validation notes live
  next to the corresponding source code.
- Voice Drop, Wrist Agent, Codex Weekly, and Find My iPhone depend on external or
  experimental components; read their project documentation before installing.
- See [CONTRIBUTING.md](CONTRIBUTING.md) before adding a project or release
  artifact.

## License

See [LICENSE](LICENSE).

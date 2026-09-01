# Testing Pebble applications

**English** · [Русский](TESTING.ru.md)

The browser is the primary way to show a watchface or watchapp for interactive
testing. Use the standard Pebble SDK emulator only when the browser workflow is
unavailable, fails to install or run the PBW, or cannot represent the behavior
being tested.

## Primary workflow: browser

Use [pebble-qemu-wasm](https://github.com/ericmigi/pebble-qemu-wasm), which boots
real PebbleOS firmware in QEMU compiled to WebAssembly.

1. Build the requested project or select the exact artifact from
   [`projects.json`](projects.json). Do not substitute an older PBW when the user
   asks to inspect current source changes.
2. Open the [PebbleOS browser emulator](https://ericmigi.github.io/pebble-qemu-wasm/).
3. Select Emery / Pebble Time 2 when the board selector is available, then press
   **Boot** and wait for PebbleOS to finish starting.
4. Press **Upload .pbw** and choose the local binary. Dragging the PBW onto the
   watch screen is equivalent. A public `apps.repebble.com` link or direct PBW
   URL can also be pasted into the install field.
5. Test with the visible watch buttons or keyboard:

   | Key | Pebble button |
   | --- | --- |
   | `↑` | Up |
   | `↓` | Down |
   | `→` or `Enter` | Select |
   | `←`, `Escape`, or `Backspace` | Back |

6. Exercise the states relevant to the request and keep the browser available
   when the user asked to see or click through the application.

The live page can also simulate battery, 12/24-hour format, shake/tap, compass,
and notifications. Browser execution is the default presentation and
interaction path, not proof of physical-device compatibility.

## Local browser hosting

For a local checkout of `pebble-qemu-wasm`:

```bash
git clone https://github.com/ericmigi/pebble-qemu-wasm.git
cd pebble-qemu-wasm
python3 server.py 8080
```

Open `http://localhost:8080`. Use the included `server.py`: it sets the
Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy headers required by
`SharedArrayBuffer`. A generic static file server is not sufficient.

## Fallback: standard Pebble emulator

Use the SDK emulator when browser boot, WebAssembly, firmware loading, PBW
upload, or application installation fails. Also use it when logs, SDK-specific
behavior, PebbleKit JS debugging, or a capability absent from the browser path
is required.

Install an existing binary:

```bash
pebble install --emulator emery --logs path/to/application.pbw
```

Or build and run from a project directory:

```bash
pebble build
pebble install --emulator emery --logs
```

If the SDK emulator is stale:

```bash
pebble kill
pebble wipe
pebble build
pebble install --emulator emery --logs
```

Report that fallback was used and why. Neither browser nor SDK emulation replaces
physical testing for microphone recording, health sensors, wakeups, phone-side
bridges, Bluetooth behavior, external services, or real credentials.

## Request interpretation

When the user asks to “show”, “open”, “run”, or “launch” a Pebble application
for testing, interpret the request as authorization to:

1. build or select the requested PBW;
2. open the browser emulator;
3. upload and install the PBW;
4. leave the interactive result available to the user.

Fall back to the standard SDK emulator only after a concrete browser-path
failure or when the requested behavior is outside browser-emulator coverage.

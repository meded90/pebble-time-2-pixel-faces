# Find My iPhone for Pebble Time 2

**English** · [Русский](README.ru.md) · [All projects](../README.md#project-gallery)

[![Find My iPhone](assets/emulator-v9-native-fit.png)](assets/emulator-v9-native-fit.png)

A native Pebble Time 2 (`emery`) watchapp. Holding Select makes the paired
iPhone send a direct HTTPS Apple Find My `playSound` request. There is no relay
server or third-party companion; the implementation uses Pebble Core and its
embedded PebbleKit JS runtime.

> Apple does not publish this API. The reverse-engineered integration can stop
> working after an Apple change, is not affiliated with Apple, and must not be
> presented as an official Find My client.

## Features

- native C watch UI with ten states, sprites, and ringing animation;
- 650 ms Select hold guard against accidental requests;
- Up/Down selection across multiple iPhones;
- direct Apple Account SRP-2048 sign-in with `s2k` and `s2k_fo`;
- trusted-device 2FA, trusted-session acquisition, and device selection;
- direct `initClient`, `accountLogin`, and `playSound` requests to Apple domains;
- one Find My session refresh retry after HTTP 450;
- distinct expired-auth, offline, rate-limit, and error states;
- ten-second local cooldown and `REQUEST_ID` duplicate protection;
- interactive watch and iPhone-settings HTML prototype;
- automatic RU, UK, EN, DE, ES, FR, IT, and PT localization, with English fallback;
- palette-audited v3 graphics and contract tests for PebbleKit JS.

## Interactive prototype

```sh
cd find-my-iphone
python3 -m http.server 49381 --bind 127.0.0.1
```

Open `http://127.0.0.1:49381/prototype/`. Watch mode implements the real hold
timer, phone selection, selectable request outcomes, and direct access to all
ten UI states. iPhone Settings mode provides a clickable demonstration of the
Apple Account → 2FA → device → active-session flow. Prototype values are local
examples and are not submitted anywhere.

## Build and test

```sh
cd find-my-iphone
npm install
npm test
../.venv/bin/pebble build
```

The local build is `build/find-my-iphone.pbw`. The audited release candidate is
[`../dist/candidates/find-my-iphone-0.1.1-emery.pbw`](../dist/candidates/find-my-iphone-0.1.1-emery.pbw).

Install in the emulator with:

```sh
../.venv/bin/pebble install --emulator emery
```

## First run

1. Install the PBW through Pebble Core and open the app settings on the iPhone.
2. Enter the Apple Account and password. The password exists only in memory for
   SRP and is cleared immediately after use.
3. Enter the six-digit trusted-device code if Apple requests 2FA.
4. Select the target iPhone and save.
5. Open the watchapp and hold Select for 650 ms.
6. “Signal sent” means Apple returned `snd.statusCode = 200`. It confirms command
   acceptance, not an acoustic callback from the phone speaker.

This version supports trusted-device 2FA codes. Requesting a separate SMS code
is not implemented.

## Authentication storage

PebbleKit JS uses Pebble Core `localStorage`, isolated by application UUID
`1ea8ffd9-0fd5-4bbe-b4b8-bf4589c94806`.

Stored locally:

- Apple Account identifier;
- `clientId`, `sessionToken`, `trustToken`, and `accountCountry`;
- `dsid`, Find My service URL, and cookie jar;
- device IDs and short names;
- selected device ID, refresh time, and last system locale.

Never stored:

- Apple Account password;
- six-digit 2FA code;
- device coordinates;
- OpenAI or other unrelated tokens.

`localStorage` is not iOS Keychain or a separate encrypted application store. A
person with access to the phone container or backup could potentially extract
session tokens. “Sign out and delete session” clears session state, pending 2FA,
the device list, and the selection.

## Data flow

```text
Pebble Select
  -> AppMessage: PLAY_SOUND + REQUEST_ID
  -> PebbleKit JS inside Pebble Core on iPhone
  -> HTTPS POST https://<fmipservice>/fmipservice/client/web/playSound
  -> Apple: snd.statusCode
  -> AppMessage: STATE + RESULT + safe ERROR_CODE
  -> Pebble screen and vibration
```

Credentials, 2FA, cookies, and session tokens never go to the watch. Pebble
receives only state, a short selected-device name/ID, and the request result.

## Domains and permissions

- `https://idmsa.apple.com` for SRP sign-in and 2FA;
- `https://setup.icloud.com` for `accountLogin` and Find My service discovery;
- the dynamic HTTPS Find My URL returned by Apple.

The app does not request location capability. Bluetooth carries AppMessage
between Pebble Core and the watch; internet access on the iPhone is required.

## Repository layout

- `prototype/` — standalone interactive HTML prototype;
- `src/c/main.c` — watch UI, controls, AppMessage, and animation;
- `src/pkjs/index.js` — app state and session persistence;
- `src/pkjs/apple.js` — Apple auth, 2FA, Find My, and `playSound`;
- `src/pkjs/crypto.js` — SRP-2048, PBKDF2-SHA256, and M1/M2 proofs;
- `src/pkjs/config-page.js` — Pebble Core settings page;
- `tests/` — contract, crypto-vector, and HTTP tests without a real account;
- `STATE_MACHINE.md` and `ASSET_MANIFEST.md` — UI and art specifications.

## Validation boundary

`npm test`, Emery PBW compilation, and the `AUTH_REQUIRED` and `READY` emulator
states are verified. Real Apple sign-in, real 2FA, and a physical phone sound
request have intentionally not been performed because they require user
credentials, a physical iPhone, and a physical Pebble Time 2. Do not enter a
real Apple password in the public browser QEMU: its PebbleKit JS CORS fallback
can route requests through the emulator author's external proxy.

# Codex Weekly

[Русский](README.md) · [English](README.en.md) · [All watchfaces](../README.en.md#native-200228-previews)

![Codex Weekly icon](../assets/icons/codex-weekly-icon-144.png)

[![Codex Weekly](../assets/screenshots/codex-weekly.png)](../assets/screenshots/codex-weekly.png)

A native Pebble Time 2 watchface (`emery`, 200×228) with:

- oversized time;
- the longest available Codex quota window;
- remaining percentage and time until reset;
- a 12-week personal usage heatmap;
- month labels and highlighted month boundaries;
- background updates through PebbleKit JS.

The square in the upper-right corner shows synchronization state: gray means
not configured, blue means updating, green means data was received, and red
means a connection or response error.

## Cloud Run setup

The project has no shared backend, preconfigured service URL, or Google Cloud
project ID. Every user deploys a private instance and enters two values in the
watchface settings: the `/status` URL and a separate client token.

Step-by-step deployment guides:

- [English](cloud-run/README.en.md)
- [Русский](cloud-run/README.md)

When data, authentication, or the service is unavailable, the watchface shows
neutral `—` values and gray blocks instead of stale or invented data.

## Local bridge alternative

Codex App Server provides `account/rateLimits/read` and
`account/usage/read`. They use the existing ChatGPT-backed Codex sign-in on
the Mac. An OpenAI Platform API key is not a replacement because Platform API
usage and personal ChatGPT/Codex limits are separate systems.

The local bridge requests only quota and daily usage data, normalizes it, and
serves compact JSON to PebbleKit JS. Its bearer token remains on the phone and
is never sent to the watch.

Cloud Run uses the same App Server but stores a copy of Codex authentication
in Secret Manager. This is experimental rather than a supported cloud OAuth
contract; review the limitation in the Cloud Run guide before using it.

## Sign into Codex

1. Sign into Codex with ChatGPT:

   ```bash
   codex login
   ```

2. Check the session:

   ```bash
   codex login status
   ```

3. Generate a separate token for the local bridge:

   ```bash
   openssl rand -hex 24
   ```

The generated value is `CODEX_PEBBLE_TOKEN`. It protects only your bridge; it
is not an OpenAI API key and should not be committed or shared.

## Run the local bridge

```bash
CODEX_PEBBLE_HOST=0.0.0.0 \
CODEX_PEBBLE_TOKEN=replace-with-generated-token \
node bridge/server.mjs
```

The bridge prints the detected network address, status URL, and health check.
The phone and Mac must be on the same network, and the Mac must remain on.
Never expose the plain HTTP bridge directly to the public internet.

Test it locally:

```bash
curl \
  -H "Authorization: Bearer replace-with-generated-token" \
  http://127.0.0.1:8765/status
```

## Build and run

```bash
npm install
pebble build
pebble install --emulator emery
```

Ready package: [`../dist/codex-weekly.pbw`](../dist/codex-weekly.pbw).

## Permissions and privacy

The watchface uses configurable PebbleKit JS networking. Keep the bridge URL
and token private. The watch receives only normalized quota and heatmap data,
not ChatGPT credentials or chat history.

## All watchfaces

[Mosaic Grid](../mosaic-grid/) · [Flip Board](../flip-board/) · [Info Tiles](../info-tiles/) · [Codex Weekly](../codex-weekly/) · [Starry Digits](../starry-digits/) · [meded90](../meded90/) · [Zodiac: Aquarius](../zodiac-aquarius/) · [Zodiac: Gemini](../zodiac-gemini/)

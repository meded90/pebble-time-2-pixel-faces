# Wrist Agent 1.0.0 release checklist

## Store metadata

- Name: **Wrist Agent**
- Type/category: watchapp / Remotes (`remotes`)
- Version: `1.0.0`
- UUID: `8c69a4e9-fe5d-4480-a2a2-db1e5b7a5af8` — never change after release
- Platform: Pebble Time 2 / `emery` only
- Price: free
- Website/source:
  `https://github.com/meded90/pebble-time-2-pixel-faces/tree/main/wrist-agent`
- Support/security contact: **required before submission**; use the real private
  contact from the developer account or a public support page. Do not invent one.
- Listing copy: [`STORE_LISTING.md`](STORE_LISTING.md)
- Release notes: [`CHANGELOG.md`](CHANGELOG.md)

## Upload files

- PBW: `dist/wrist-agent.pbw`
- 80×80 small icon: `assets/icons/wrist-agent-icon-80.png`
- 144×144 icon: `assets/icons/wrist-agent-icon-144.png`
- 512×512 source icon: `assets/icons/wrist-agent-icon-512.png`
- 720×320 banner: `assets/banners/wrist-agent-banner-720x320.png`
- Primary 200×228 screenshot: `assets/screenshots/wrist-agent.png`
- Result screenshot: `assets/screenshots/wrist-agent-result.png`
- First-run/setup screenshot: `assets/screenshots/wrist-agent-setup.png`

## Reproducible checks

```bash
swift scripts/generate-app-icons.swift
cd wrist-agent
npm ci
npm test
../.venv/bin/pebble clean
../.venv/bin/pebble build
unzip -t build/wrist-agent.pbw
node --check build/pebble-js-app.js
cp build/wrist-agent.pbw ../dist/wrist-agent.pbw

cd ../wrist-agent-server
npm ci
npm run check
npm test
npm audit --omit=dev
```

Verify exact image dimensions with `sips -g pixelWidth -g pixelHeight`. The
current portal must accept the PBW upload; its API does not publish a fixed PBW
size limit that this checklist can prove offline.

## Emulator matrix

- [x] Install and open on `emery`.
- [x] First-run setup-needed screen is readable.
- [x] Ready screen is readable at 200×228.
- [x] System Dictation accepts a synthetic transcript.
- [x] Completed result and action summary fit and scroll.
- [ ] Synthetic `connectivity`, `disabled`, and `no-speech-detected` screens.
- [ ] `needs_confirmation` approve and Back/reject over a running local bridge.
- [ ] App restart during a pending POST reuses the same idempotency key.

## Real-device and live-service gates

Do not publish until these external checks are complete:

- [ ] Physical Pebble Time 2 microphone and the target Core Devices build.
- [ ] Dictation in every language promised by the listing.
- [ ] Bluetooth loss and phone network loss.
- [ ] Public HTTPS `/healthz` and Streamable HTTP `/mcp`.
- [ ] MCP Inspector initialization, tools/list, valid callback, invalid callback.
- [ ] Published `agtch_...` channel with a real Workspace Agent access token.
- [ ] Read-only connected-app query returns a result to the watch.
- [ ] Proposed calendar write returns `needs_confirmation` before mutation.
- [ ] Select approval performs exactly one action; retry creates no duplicate.
- [ ] Back reject performs no action.
- [ ] Upstream `suspended` tells the user to open ChatGPT without claiming success.
- [ ] Privacy/support URLs are public and match the listing.

## Submission boundary

This repository prepares the PBW and materials but does not submit them. Review
the final listing preview and click Publish manually in the RePebble Developer
Dashboard. OpenAI plugin public-directory submission is not required for the
private callback plugin; if it is pursued separately, implement OAuth 2.1 and
meet the then-current OpenAI review requirements.

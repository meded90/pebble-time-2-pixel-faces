# Changelog

## 1.0.10 — 2026-08-31

- Added direct Russian and English Cloud Run deployment-guide links to the settings page.

## 1.0.9 — 2026-08-31

- Added a settings-page button that checks the configured server URL and client token without saving them.
- Show the reason for a failed sync next to the top-right status square in the same pixel font as `CODEX`, right-aligned before the square.
- Clear stale quota values when the watch-to-phone sync message cannot be delivered.

## 1.0.8 — 2026-08-21

- Show the Cloud Run state or a safe error code next to the sync indicator.
- Clear persisted quota values after a server failure so the watchface shows `—` and gray cells instead of stale data.
- Return safe `AUTH`, `TIME`, `DATA`, or `ERR` codes from the Cloud Run endpoint.

## 1.0.7 — 2026-08-18

- Removed the API-equivalent token cost from the watchface.
- Removed the local bridge dependency on T3 Code usage and pricing caches.
- Restored the quota panel to the remaining percentage and reset countdown.

## 1.0.6 — 2026-08-18

- Added an experimental API-equivalent token cost for the active Codex quota period.

## 1.0.5 — 2026-08-12

- Added reusable Cloud Run deployment scripts and step-by-step guides in Russian and English.
- Removed the preconfigured Google Cloud project and service URL so every user connects their own deployment.
- Documented the experimental authentication boundary, secure token handling, and neutral no-data behavior.

## 1.0.4 — 2026-08-03

- Reduced application icon padding for better readability in the watch menu.

## 1.0.3 — 2026-08-03

- Made the sync status square a solid color with no white border.
- Show a dash and neutral gray quota and usage cells until the first successful sync.
- Read only the main Codex quota, ignoring model-specific limits such as GPT-5.3-Codex-Spark.
- Added temporary bridge diagnostics for the available and selected quota windows.

## 1.0.2 — 2026-08-03

- Made the reset countdown brighter and larger.
- Made month labels brighter, larger, and easier to scan.
- Replaced the `88:88` app icon title with centered pixel text `CODEX`.

## 1.0.1 — 2026-08-03

- Expanded Personal Usage to the full display width.
- Added aligned month labels and yellow month boundaries.
- Connected the top status square to live synchronization states.
- Added Pebble and RePebble application icons.

## 1.0.0 — 2026-08-03

- Initial Codex Weekly release for Pebble Time 2.

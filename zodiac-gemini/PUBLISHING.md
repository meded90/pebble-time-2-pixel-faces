# RePebble publishing checklist

## Publication status

- **Status:** Published
- **Published:** 2026-08-11
- **App ID:** `a1c61f1227144535accbf53c`
- **Public page:** https://apps.repebble.com/a1c61f1227144535accbf53c
- **Published release:** `1.3.1`

## Local release candidate

- **Version:** `1.4.0`
- **Status:** built and emulator-checked, not published
- **Change:** hand-drawn sprite digits at their original pixel scale
- **Layout:** hours above minutes in the upper-left corner
- **Resource use:** 12,072 bytes of the 256 KB Emery resource budget

## Listing fields

- **App Name:** Zodiac: Gemini
- **Category:** Faces
- **Compatibility:** Pebble Time 2 (`emery`)
- **Website:** https://github.com/meded90/pebble-time-2-pixel-faces/tree/main/zodiac-gemini
- **Source Code URL:** https://github.com/meded90/pebble-time-2-pixel-faces/tree/main/zodiac-gemini
- **Companion apps:** none
- **Permissions:** none

## Description

Zodiac: Gemini pairs a vivid twin portrait with a large, rounded digital clock
designed specifically for the 200×228 Pebble Time 2 display. Hours and minutes
are stacked in a balanced card in the upper-left corner, leaving the original
illustration clear and expressive.

The watchface follows the system 12/24-hour preference, updates every minute
and works completely offline. It has no settings, companion app, analytics or
network permissions.

## Version 1.3.1 release notes

Added a dedicated Pebble launcher icon and the complete RePebble artwork set.
The illustration now keeps its clean red palette and soft light-gray edge,
while the larger rounded clock is easier to read.

## Upload files

- **PBW:** `dist/candidates/zodiac-gemini-1.4.0.pbw`
- **Screenshot (exactly 200×228):** `assets/screenshots/zodiac-gemini.png`
- **Banner (exactly 720×320):** `assets/banners/zodiac-gemini-banner-720x320.png`
- **Small Icon (80×80):** `assets/icons/zodiac-gemini-icon-80.png`
- **Large Icon (144×144):** `assets/icons/zodiac-gemini-icon-144.png`

Additional source assets are available as a 32×32 icon, a 512×512 master and
the bundled 25×25 Pebble menu icon. The PBW, listing text, screenshot, banner
and both application icons were reviewed before publication.

## Verification before publishing

- Confirm the PBW is below RePebble's 4.4 MB upload limit.
- Confirm the manifest version is `1.3.1` and the target platform is `emery`.
- Confirm the package contains the 25×25 menu icon and the 200×228 background.
- Open the screenshot, banner and both store icons at their native size.
- Install the release PBW on the Emery emulator or a Pebble Time 2 and check the
  watchface plus launcher icon before enabling **Published**.

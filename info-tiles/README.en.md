# Info Tiles

[Русский](README.md) · [English](README.en.md) · [All watchfaces](../README.en.md#native-200228-previews)

[![Info Tiles](../assets/screenshots/info-tiles.png)](../assets/screenshots/info-tiles.png)

An information-focused Pebble Time 2 watchface (`emery`, 200×228) that places
the most useful metrics in large, high-contrast tiles.

## Features

- time and date using the system 12/24-hour preference;
- watch battery percentage;
- current temperature and Open-Meteo weather code;
- today's steps and the latest available heart-rate value from Pebble Health;
- weather refresh through the connected phone approximately every 30 minutes;
- safe `--` placeholders when GPS, networking, or Health data is unavailable.

## Build and run

```bash
pebble build
pebble install --emulator emery --logs
```

Ready package: [`../dist/info-tiles.pbw`](../dist/info-tiles.pbw).

## Permissions and privacy

Phone location and Pebble Health are required. Coordinates go directly to
Open-Meteo for weather lookup; Health values are read locally and are not
uploaded by the watchface.

## All watchfaces

[Mosaic Grid](../mosaic-grid/) · [Flip Board](../flip-board/) · [Info Tiles](../info-tiles/) · [Codex Weekly](../codex-weekly/) · [Starry Digits](../starry-digits/) · [meded90](../meded90/) · [Zodiac: Aquarius](../zodiac-aquarius/) · [Zodiac: Gemini](../zodiac-gemini/)

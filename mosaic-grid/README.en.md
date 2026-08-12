# Mosaic Grid

[Русский](README.md) · [English](README.en.md) · [All watchfaces](../README.en.md#native-200228-previews)

[![Mosaic Grid](../assets/screenshots/mosaic-grid.png)](../assets/screenshots/mosaic-grid.png)

A native Pebble Time 2 watchface (`emery`, 200×228). Large pixel digits sit
inside a strict modernist grid of contrasting color blocks.

## Features

- hours and minutes with the system 12/24-hour preference;
- weekday and day of month;
- current watch battery percentage;
- fully offline operation with no phone connection;
- minute-based time updates and event-based battery updates.

## Build and run

```bash
pebble build
pebble install --emulator emery --logs
```

Ready package: [`../dist/mosaic-grid.pbw`](../dist/mosaic-grid.pbw).

## Permissions

None. The watchface does not use networking, location, or Pebble Health.

## All watchfaces

[Mosaic Grid](../mosaic-grid/) · [Flip Board](../flip-board/) · [Info Tiles](../info-tiles/) · [Codex Weekly](../codex-weekly/) · [Starry Digits](../starry-digits/) · [meded90](../meded90/) · [Zodiac: Aquarius](../zodiac-aquarius/) · [Zodiac: Gemini](../zodiac-gemini/)

# Flip Board

[Русский](README.md) · [English](README.en.md) · [All watchfaces](../README.en.md#native-200228-previews)

[![Flip Board](../assets/screenshots/flip-board.png)](../assets/screenshots/flip-board.png)

A mechanical flip clock for Pebble Time 2 (`emery`, 200×228) with four large
panels and a flip animation for digits that change.

## Features

- system 12/24-hour preference;
- weekday, date, month, and battery percentage;
- animated central hinge on each flip panel;
- curated themes and manual background, line, and digit colors through Clay;
- persistent color settings stored on the watch;
- offline operation after settings are saved.

## Build and run

```bash
npm install
pebble build
pebble install --emulator emery --logs
```

Ready package: [`../dist/flip-board.pbw`](../dist/flip-board.pbw).

## Permissions and privacy

No network, location, or Pebble Health access. The phone sends only the colors
selected by the user.

## All watchfaces

[Mosaic Grid](../mosaic-grid/) · [Flip Board](../flip-board/) · [Info Tiles](../info-tiles/) · [Codex Weekly](../codex-weekly/) · [Starry Digits](../starry-digits/) · [meded90](../meded90/) · [Zodiac: Aquarius](../zodiac-aquarius/) · [Zodiac: Gemini](../zodiac-gemini/)

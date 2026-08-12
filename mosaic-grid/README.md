# Mosaic Grid

[Русский](README.md) · [English](README.en.md) · [Все циферблаты](../README.md#нативные-превью-200228)

[![Mosaic Grid](../assets/screenshots/mosaic-grid.png)](../assets/screenshots/mosaic-grid.png)

Нативный циферблат для Pebble Time 2 (`emery`, 200×228). Крупные пиксельные
часы встроены в строгую модернистскую сетку с контрастными цветовыми блоками.

## Возможности

- часы и минуты с поддержкой системного формата 12/24 часа;
- день недели и число;
- текущий заряд часов в процентах;
- полностью автономная работа без телефона и сети;
- обновление времени раз в минуту и заряда по событию PebbleOS.

## Сборка и запуск

```bash
pebble build
pebble install --emulator emery --logs
```

Готовый пакет: [`../dist/mosaic-grid.pbw`](../dist/mosaic-grid.pbw).

## Разрешения

Не требуются. Циферблат не использует сеть, геолокацию или Pebble Health.

## Все циферблаты

[Mosaic Grid](../mosaic-grid/) · [Flip Board](../flip-board/) · [Info Tiles](../info-tiles/) · [Codex Weekly](../codex-weekly/) · [Starry Digits](../starry-digits/) · [meded90](../meded90/) · [Zodiac: Aquarius](../zodiac-aquarius/) · [Zodiac: Gemini](../zodiac-gemini/)

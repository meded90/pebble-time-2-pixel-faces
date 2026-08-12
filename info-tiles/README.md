# Info Tiles

[Русский](README.md) · [English](README.en.md) · [Все циферблаты](../README.md#нативные-превью-200228)

[![Info Tiles](../assets/screenshots/info-tiles.png)](../assets/screenshots/info-tiles.png)

Информационный циферблат для Pebble Time 2 (`emery`, 200×228), который
собирает основные показатели в крупные контрастные плитки.

## Возможности

- время и дата с системным форматом 12/24 часа;
- заряд часов;
- текущая температура и погодный код Open-Meteo;
- шаги за сегодня и последнее доступное значение пульса из Pebble Health;
- обновление погоды через подключённый телефон примерно раз в 30 минут;
- безопасные значения `--`, если GPS, сеть или Health недоступны.

## Сборка и запуск

```bash
pebble build
pebble install --emulator emery --logs
```

Готовый пакет: [`../dist/info-tiles.pbw`](../dist/info-tiles.pbw).

## Разрешения и приватность

Нужны геолокация телефона и Pebble Health. Координаты отправляются напрямую в
Open-Meteo только для погоды; данные Health читаются локально и циферблатом не
загружаются на сторонний сервер.

## Все циферблаты

[Mosaic Grid](../mosaic-grid/) · [Flip Board](../flip-board/) · [Info Tiles](../info-tiles/) · [Codex Weekly](../codex-weekly/) · [Starry Digits](../starry-digits/) · [meded90](../meded90/) · [Zodiac: Aquarius](../zodiac-aquarius/) · [Zodiac: Gemini](../zodiac-gemini/)

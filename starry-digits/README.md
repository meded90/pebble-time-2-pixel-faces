# Starry Digits

[Русский](README.md) · [English](README.en.md) · [Все циферблаты](../README.md#нативные-превью-200228)

[![Starry Digits](../assets/screenshots/starry-digits.png)](../assets/screenshots/starry-digits.png)

Художественный циферблат для Pebble Time 2 (`emery`, 200×228) и Pebble Round 2
(`gabbro`, 260×260): светящиеся рисованные цифры поверх звёздного пиксельного
неба с вихревыми мазками.

## Возможности

- собственные растровые изображения всех десяти цифр;
- отдельные фон и безопасная компоновка для круглого экрана 260×260;
- вертикальная композиция часов и минут;
- системный формат времени 12/24 часа;
- обновление раз в минуту;
- полностью автономная работа без настроек, телефона и сети.

## Сборка и запуск

```bash
pebble build
pebble install --emulator emery --logs
pebble install --emulator gabbro --logs
```

Готовый пакет: [`../dist/starry-digits.pbw`](../dist/starry-digits.pbw).

## Разрешения

Не требуются.

## Все циферблаты

[Mosaic Grid](../mosaic-grid/) · [Flip Board](../flip-board/) · [Info Tiles](../info-tiles/) · [Codex Weekly](../codex-weekly/) · [Starry Digits](../starry-digits/) · [meded90](../meded90/) · [Zodiac: Aquarius](../zodiac-aquarius/) · [Zodiac: Gemini](../zodiac-gemini/)

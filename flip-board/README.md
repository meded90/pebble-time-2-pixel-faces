# Flip Board

[Русский](README.md) · [English](README.en.md) · [Все циферблаты](../README.md#нативные-превью-200228)

[![Flip Board](../assets/screenshots/flip-board.png)](../assets/screenshots/flip-board.png)

Механический flip-clock для Pebble Time 2 (`emery`, 200×228) с четырьмя
крупными панелями и анимацией переворота изменившихся цифр.

## Возможности

- системный формат времени 12/24 часа;
- день недели, число, месяц и заряд батареи;
- анимированный центральный шарнир flip-панелей;
- готовые цветовые темы и ручная настройка фона, линий и цифр через Clay;
- сохранение выбранных цветов в постоянной памяти часов;
- автономная работа после сохранения настроек.

## Сборка и запуск

```bash
npm install
pebble build
pebble install --emulator emery --logs
```

Готовый пакет: [`../dist/flip-board.pbw`](../dist/flip-board.pbw).

## Разрешения и приватность

Сеть, геолокация и Pebble Health не используются. Телефон передаёт на часы
только выбранные пользователем цвета.

## Все циферблаты

[Mosaic Grid](../mosaic-grid/) · [Flip Board](../flip-board/) · [Info Tiles](../info-tiles/) · [Codex Weekly](../codex-weekly/) · [Starry Digits](../starry-digits/) · [meded90](../meded90/) · [Zodiac: Aquarius](../zodiac-aquarius/) · [Zodiac: Gemini](../zodiac-gemini/)

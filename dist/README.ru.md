# Release artifacts

[English](README.md) · **Русский**

PBW разделены по статусу самого бинарника. «Опубликованная версия» означает совпадение `versionLabel` с проверенным публичным релизом; идентичность байтов со скачиванием Appstore без отдельной проверки не заявляется. Более новая локальная сборка остаётся релиз-кандидатом.

| Проект | Артефакт | Версия | Статус бинарника | Статус в магазине |
| --- | --- | --- | --- | --- |
| [Gym Zones](../gym-zones/) | [candidates/gym-zones-1.0.0.pbw](candidates/gym-zones-1.0.0.pbw) | 1.0.0 | Релиз-кандидат | Не опубликован |
| [Info Tiles](../info-tiles/) | [candidates/info-tiles-1.0.0.pbw](candidates/info-tiles-1.0.0.pbw) | 1.0.0 | Релиз-кандидат | Не опубликован |
| [meded90](../meded90/) | [candidates/meded90-1.0.0.pbw](candidates/meded90-1.0.0.pbw) | 1.0.0 | Релиз-кандидат | Не опубликован |
| [Wrist Agent](../wrist-agent/) | [candidates/wrist-agent-1.0.0.pbw](candidates/wrist-agent-1.0.0.pbw) | 1.0.0 | Релиз-кандидат | Не опубликован |
| [Zodiac: Aquarius](../zodiac-aquarius/) | [candidates/zodiac-aquarius-1.3.0.pbw](candidates/zodiac-aquarius-1.3.0.pbw) | 1.3.0 | Релиз-кандидат | Не опубликован |
| [Zodiac: Gemini](../zodiac-gemini/) | [candidates/zodiac-gemini-1.4.0.pbw](candidates/zodiac-gemini-1.4.0.pbw) | 1.4.0 | Релиз-кандидат | [1.3.1](https://apps.repebble.com/zodiac-gemini_a1c61f1227144535accbf53c) |
| [Voice Drop](../voice-drop/) | [experimental/voice-drop-prototype-0.1.0.pbw](experimental/voice-drop-prototype-0.1.0.pbw) | 0.1.0 | Экспериментальный прототип | Не опубликован |
| [Codex Weekly](../codex-weekly/) | [published/codex-weekly-1.0.7.pbw](published/codex-weekly-1.0.7.pbw) | 1.0.7 | Бинарник опубликованной версии | [1.0.7](https://apps.repebble.com/codex-weekly_27f48d86803e471a83b93dfe) |
| [Find My iPhone](../find-my-iphone/) | [published/find-my-iphone-0.1.4-emery.pbw](published/find-my-iphone-0.1.4-emery.pbw) | 0.1.4 | Бинарник опубликованной версии | [0.1.4](https://apps.repebble.com/7ffae9b84d714b9882f7a40f) |
| [Flip Board](../flip-board/) | [published/flip-board-1.2.1.pbw](published/flip-board-1.2.1.pbw) | 1.2.1 | Бинарник опубликованной версии | [1.2.1](https://apps.repebble.com/flip-board_d9b87c9f10a74d718db82b06) |
| [Mosaic Grid](../mosaic-grid/) | [published/mosaic-grid-1.0.2.pbw](published/mosaic-grid-1.0.2.pbw) | 1.0.2 | Бинарник опубликованной версии | [1.0.2](https://apps.repebble.com/mosaic-grid_d2ccde5d2187490085b44f8e) |
| [Starry Digits](../starry-digits/) | [published/starry-digits-1.1.0.pbw](published/starry-digits-1.1.0.pbw) | 1.1.0 | Бинарник опубликованной версии | [1.1.0](https://apps.repebble.com/starry-digits_bacf5a80f08845558f44cf65) |

`experimental/voice-drop-prototype-0.1.0.pbw` — старый прототип записи через телефон, несовместимый с текущими исходниками Voice Drop 0.2.0.

Проверяйте каждый файл по [SHA256SUMS](SHA256SUMS). Для обновления метаданных каталога выполните `npm run projects:sync`.

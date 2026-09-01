# RePebble deployment package

[English](README.md) · **Русский**

Материалы подготовлены для публикации watchapp `Find My iPhone` на платформе `emery`.

## Обязательные файлы

- `icons/icon-48.png` — Small Icon, 48×48;
- `icons/icon-144.png` — Large Icon, 144×144;
- `banner/marketing-banner-720x320.png` — обязательный баннер watchapp;
- `screenshots/emery/*.png` — пять необрамлённых скриншотов 200×228;
- `../../dist/candidates/find-my-iphone-0.1.0-emery.pbw` — бинарник релиз-кандидата;
- `description.en.txt` и `release-notes.en.txt` — готовые поля листинга;
- `metadata.json` — все поля в удобном для переноса виде.

Дополнительно: `icons/menu-icon-25.png`, `icon-80.png`, `icon-512.png`, русские тексты, расширенные `listing.*.md` и общий `store-assets-preview.png`.

## Перед публикацией

1. Укажите support email вместо `TODO` в `metadata.json` или непосредственно в портале.
2. Проверьте, что URL исходников уже доступен публично: локальные изменения этого проекта пока не публикуются автоматически.
3. Протестируйте вход, 2FA и реальный вызов звука на физическом iPhone и Pebble Time 2.
4. Повторно оцените предупреждения из `SECURITY.md`: приложение использует недокументированный Apple API и хранит сессионные токены в `localStorage` Pebble Core, а не в Keychain.
5. Сначала создайте скрытый/private listing для проверки. Финальное нажатие Publish — отдельное публичное действие и в этот пакет не входит.

## Пересборка графики

```sh
swift tools/build-store-assets.swift
```

ImageGen-мастера хранятся в `developer-portal/source/`. Скриншоты получены из точных состояний HTML-прототипа, затем приведены к нативному размеру Emery.

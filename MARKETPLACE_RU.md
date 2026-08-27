# Как выпустить циферблаты в Pebble Appstore

Инструкция актуализирована для Pebble SDK 4.17 и Pebble Time 2 (`emery`) на
29 июля 2026 года.

Для self-hosted watchapp Wrist Agent используйте более строгий отдельный
checklist: [`wrist-agent/PUBLISHING.md`](wrist-agent/PUBLISHING.md). Он включает
PBW, иконки, screenshots, private bridge, MCP callback, live Workspace Agent и
реальный тест подтверждения календарного действия.

## 1. Подготовить окружение

На macOS:

```bash
brew install node libpng uv
uv tool install pebble-tool
pebble sdk install latest
```

Проверьте:

```bash
pebble --version
pebble sdk list
```

## 2. Проверить метаданные

Откройте `package.json` каждого циферблата и проверьте:

- `author` — имя, которое должно отображаться у приложения;
- `version` — формат `major.minor.0`, для первого релиза оставьте `1.0.0`;
- `pebble.displayName` — имя карточки в Appstore;
- `pebble.uuid` — не меняйте после первого релиза;
- `targetPlatforms` содержит `emery`;
- `watchapp.watchface` установлен в `true`.

`info-tiles` честно объявляет разрешения `health` и `location`. Они нужны для
пульса/шагов и GPS-погоды; Appstore отклонит использование Health API без
capability `health`.

## 3. Собрать релизные PBW

Из корня проекта:

```bash
make clean
make build
```

Или по одному:

```bash
cd mosaic-grid
pebble clean
pebble build
```

Повторите для `flip-board` и `info-tiles`. Загрузке в Appstore подлежит файл
`.pbw` из `build/`, не исходники и не ZIP проекта.

## 4. Проверить на эмуляторе и часах

Для Time 2 используйте платформу `emery`:

```bash
cd mosaic-grid
pebble install --emulator emery
pebble screenshot screenshot-mosaic.png
```

Проверьте минимум:

- 12- и 24-часовой режим;
- даты с однозначным и двузначным числом;
- заряд 0%, 9%, 100% и режим зарядки;
- отсутствие обрезки на 200×228;
- `info-tiles`: разрешённый и запрещённый GPS, отсутствие сети, выключенный
  Pebble Health, реальные шаги и пульс;
- запуск после перезагрузки часов и после потери связи с телефоном.

Для установки на реальные часы в новом Pebble mobile app откройте
`Devices → ⋯ → Enable Dev Connect`, войдите через GitHub, затем:

```bash
pebble login
pebble install --cloudpebble
pebble logs --cloudpebble
```

## 5. Подготовить материалы карточки

Для каждого циферблата подготовьте отдельный набор:

- название и краткое описание на английском;
- подробное описание функций;
- иконка приложения;
- минимум один нативный скриншот `emery` 200×228;
- URL поддержки или контактный email;
- URL исходников, если хотите открыть код;
- заметка о данных для `info-tiles`: GPS используется только для запроса
  погоды, координаты передаются напрямую Open‑Meteo; шаги и пульс читаются
  локально с часов.

Черновики английских описаний уже подготовлены в
[`STORE_LISTINGS.md`](STORE_LISTINGS.md).

Рекомендуемые названия карточек:

- **Mosaic Grid**
- **Flip Board**
- **Info Tiles**

## 6. Загрузить в Developer Dashboard

1. Откройте [Pebble Developer Dashboard](https://developer.repebble.com/dashboard).
2. Войдите через GitHub.
3. Создайте новую запись watchface.
4. Загрузите соответствующий `.pbw`.
5. Заполните описание, контакты и изображения.
6. Укажите Pebble Time 2 / `emery` как поддерживаемую платформу.
7. Просмотрите предпросмотр карточки и опубликуйте.
8. Повторите процесс ещё два раза: у проектов разные UUID, это три отдельных
   приложения.

Публикация должна быть бесплатной, если вы отдельно не подключаете допустимую
Marketplace-модель монетизации. Не обещайте поддержку старых Pebble: текущая
версия этих проектов целенаправленно собрана только для `emery`.

## 7. Выпуск обновлений

Для обновления существующей карточки:

1. Не меняйте UUID.
2. Поднимите `version`, например `1.0.0` → `1.1.0`.
3. Выполните чистую сборку.
4. Проверьте PBW на `emery`.
5. В той же карточке Dashboard загрузите новый PBW и добавьте changelog.

## Официальные источники

- [Установка Pebble SDK](https://developer.repebble.com/sdk/)
- [Метаданные приложения](https://developer.repebble.com/guides/tools-and-resources/app-metadata/)
- [Pebble CLI](https://developer.repebble.com/guides/tools-and-resources/pebble-tool/)
- [Погода через Open‑Meteo](https://developer.repebble.com/tutorials/watchface-tutorial/part4/)
- [Pebble Health](https://developer.repebble.com/guides/events-and-services/health/)

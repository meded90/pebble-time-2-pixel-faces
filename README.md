# Pebble Time 2 Pixel Faces

Три самостоятельных watchface-приложения для Pebble Time 2 (`emery`,
200×228, 64 цвета), созданные по выделенным референсам:

1. `mosaic-grid` — крупные часы и строгая цветовая сетка.
2. `flip-board` — тёмное механическое табло с четырьмя flip-панелями.
3. `info-tiles` — время, дата, заряд, погода, пульс и шаги.

## Нативные превью 200×228

| Mosaic Grid | Flip Board | Info Tiles |
| --- | --- | --- |
| ![Mosaic Grid](assets/screenshots/mosaic-grid.png) | ![Flip Board](assets/screenshots/flip-board.png) | ![Info Tiles](assets/screenshots/info-tiles.png) |

Каждая папка является отдельным Pebble-проектом со своим UUID и собирается в
отдельный `.pbw`. Это важно: в Appstore они публикуются как три отдельные
карточки.

## Быстрый старт

Установите Pebble CLI и SDK:

```bash
brew install node libpng uv
uv tool install pebble-tool
pebble sdk install latest
```

Соберите все три проекта:

```bash
make build
```

Готовые пакеты появятся внутри:

```text
mosaic-grid/build/*.pbw
flip-board/build/*.pbw
info-tiles/build/*.pbw
```

Проверенные релизные копии уже собраны в `dist/`:

```text
dist/mosaic-grid.pbw
dist/flip-board.pbw
dist/info-tiles.pbw
```

Установка на Pebble Time 2 через новый Pebble mobile app:

```bash
pebble login
cd mosaic-grid
pebble install --cloudpebble
```

Для двух остальных циферблатов повторите команду из их каталогов.

Подробный процесс публикации: [MARKETPLACE_RU.md](MARKETPLACE_RU.md).
Готовые тексты карточек: [STORE_LISTINGS.md](STORE_LISTINGS.md).

## Особенности

- Циферблаты рассчитаны нативно на 200×228, а не запускаются в bezel mode.
- Время обновляется раз в минуту, заряд — по событию Battery State Service.
- `info-tiles` использует Pebble Health для шагов и пульса.
- `info-tiles` запрашивает GPS телефона и текущую погоду у Open‑Meteo каждые
  30 минут; API-ключ не требуется.
- При недоступных Health/GPS/сети интерфейс показывает `--`, не падает.

## Перед публикацией

В каждом `package.json` замените `author`, если имя разработчика должно
отличаться от `Kirill Baldin`. UUID уже уникальны — после публикации их нельзя
менять, иначе Appstore воспримет сборку как новое приложение.

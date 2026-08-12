# Pebble Time 2 Pixel Faces

Также в репозитории есть `voice-drop` — отдельное автономное watchapp-приложение,
которое пишет звук микрофоном Pebble Time 2, а не телефоном. Для него нужны патч
PebbleOS, общий iOS/Android-мост в приложении Core Devices и небольшой Telegram
сервер. Начните с [`voice-drop/README.md`](voice-drop/README.md), затем смотрите
[`voice-drop-firmware/README.md`](voice-drop-firmware/README.md) и
[`voice-drop-companion/README.md`](voice-drop-companion/README.md).

Основная подборка включает шесть самостоятельных watchface-приложений для
Pebble Time 2 (`emery`,
200×228, 64 цвета), созданные по выделенным референсам:

1. `mosaic-grid` — крупные часы и строгая цветовая сетка.
2. `flip-board` — тёмное механическое табло с четырьмя flip-панелями.
3. `info-tiles` — время, дата, заряд, погода, пульс и шаги.
4. `codex-weekly` — крупные часы, недельный лимит Codex и heatmap личного
   использования.
5. `starry-digits` — рукописные бело-голубые цифры на живописном звёздном
   фоне с вихревыми масляными мазками.
6. `meded90` — пиксельный портрет в красном пиджаке и вертикальные часы,
   размещённые в свободной области фона.

## Нативные превью 200×228

| Mosaic Grid | Flip Board | Info Tiles | Codex Weekly |
| --- | --- | --- | --- |
| [![Mosaic Grid](assets/screenshots/mosaic-grid.png)](mosaic-grid/) | [![Flip Board](assets/screenshots/flip-board.png)](flip-board/) | [![Info Tiles](assets/screenshots/info-tiles.png)](info-tiles/) | [![Codex Weekly](assets/screenshots/codex-weekly.png)](codex-weekly/) |

| Starry Digits |
| --- |
| [![Starry Digits](assets/screenshots/starry-digits.png)](starry-digits/) |

| meded90 |
| --- |
| [![meded90](assets/screenshots/meded90.png)](meded90/) |

Каждая папка является отдельным Pebble-проектом со своим UUID и собирается в
отдельный `.pbw`. Это важно: в Appstore они публикуются как шесть отдельных
карточки.

## Быстрый старт

Установите Pebble CLI и SDK:

```bash
brew install node libpng uv
uv tool install pebble-tool
pebble sdk install latest
```

Соберите все шесть проектов:

```bash
make build
```

Готовые пакеты появятся внутри:

```text
mosaic-grid/build/*.pbw
flip-board/build/*.pbw
info-tiles/build/*.pbw
codex-weekly/build/*.pbw
starry-digits/build/*.pbw
meded90/build/*.pbw
```

## Локальная разработка и тестирование

Отдельного веб-сервера у Pebble-проектов нет. Роль dev server выполняет
эмулятор из Pebble SDK: CLI собирает watchface, запускает Pebble Time 2
(`emery`) и устанавливает в него свежий `.pbw`.

Проверьте, что CLI и SDK доступны:

```bash
pebble --version
pebble sdk list
```

Запустите один из watchface, например `mosaic-grid`:

```bash
cd mosaic-grid
pebble build
pebble install --emulator emery --logs
```

Откроется окно эмулятора, а в терминале появятся логи приложения. Чтобы
проверить изменения в коде, остановите вывод логов через `Ctrl+C`, снова
соберите проект и переустановите его в уже запущенный эмулятор:

```bash
pebble build
pebble install --emulator emery --logs
```

Для запуска другого watchface перейдите из корня репозитория в
`flip-board`, `info-tiles` или `codex-weekly` и выполните те же две команды.

Полезные команды для проверки состояний:

```bash
# Переключить формат времени
pebble emu-time-format --emulator emery --format 12h
pebble emu-time-format --emulator emery --format 24h

# Проверить уровни заряда
pebble emu-battery --emulator emery --percent 9
pebble emu-battery --emulator emery --percent 100 --charging

# Данные Pebble Health для info-tiles
pebble emu-steps --emulator emery 12345
pebble emu-heart-rate --emulator emery 72

# Сохранить скриншот текущего экрана
pebble screenshot --emulator emery screenshot.png
```

Если эмулятор завис, показывает старую сборку или пустой экран, сбросьте его и
установите watchface заново:

```bash
pebble kill
pebble wipe
pebble build
pebble install --emulator emery --logs
```

`pebble wipe` удаляет данные эмулятора, но не исходники проекта.

Проверенные релизные копии уже собраны в `dist/`:

```text
dist/mosaic-grid.pbw
dist/flip-board.pbw
dist/info-tiles.pbw
dist/codex-weekly.pbw
dist/starry-digits.pbw
dist/meded90.pbw
dist/voice-drop.pbw
```

`dist/voice-drop.pbw` относится к старому прототипу с записью через телефон.
Версия `0.2.0` требует SDK revision 110 из пропатченной PebbleOS; до такой сборки
старый PBW не следует устанавливать.

Установка на Pebble Time 2 через новый Pebble mobile app:

```bash
pebble login
cd mosaic-grid
pebble install --cloudpebble
```

Для остальных циферблатов повторите команду из их каталогов.

Подробный процесс публикации: [MARKETPLACE_RU.md](MARKETPLACE_RU.md).
Готовые тексты карточек: [STORE_LISTINGS.md](STORE_LISTINGS.md).

## Особенности

- Циферблаты рассчитаны нативно на 200×228, а не запускаются в bezel mode.
- Время обновляется раз в минуту, заряд — по событию Battery State Service;
  в `flip-board` изменившиеся карточки механически переворачиваются через
  центральный шарнир.
- `flip-board` позволяет выбрать готовую цветовую тему или вручную настроить
  цвета фона, линий и цифр через мобильное приложение Pebble.
- `info-tiles` использует Pebble Health для шагов и пульса.
- `info-tiles` запрашивает GPS телефона и текущую погоду у Open‑Meteo каждые
  30 минут; API-ключ не требуется.
- `codex-weekly` получает недельный лимит и 12-недельную heatmap через
  локальный Codex bridge; инструкция по безопасному подключению находится в
  [`codex-weekly/README.md`](codex-weekly/README.md).
- При недоступных Health/GPS/сети интерфейс показывает `--`, не падает.

## Перед публикацией

В каждом `package.json` замените `author`, если имя разработчика должно
отличаться от `Kirill Baldin`. UUID уже уникальны — после публикации их нельзя
менять, иначе Appstore воспримет сборку как новое приложение.

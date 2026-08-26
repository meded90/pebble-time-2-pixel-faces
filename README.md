# Pebble Time 2 Pixel Faces

[Русский](README.md) · [English](README.en.md)

Также в репозитории есть `voice-drop` — отдельное автономное watchapp-приложение,
которое пишет звук микрофоном Pebble Time 2, а не телефоном. Для него нужны патч
PebbleOS, общий iOS/Android-мост в приложении Core Devices и небольшой Telegram
сервер. Начните с [`voice-drop/README.md`](voice-drop/README.md), затем смотрите
[`voice-drop-firmware/README.md`](voice-drop-firmware/README.md) и
[`voice-drop-companion/README.md`](voice-drop-companion/README.md).

Основная подборка включает восемь самостоятельных watchface-приложений для
Pebble Time 2 (`emery`,
200×228, 64 цвета), созданные по выделенным референсам:

1. [Mosaic Grid](mosaic-grid/) — крупные часы и строгая цветовая сетка.
2. [Flip Board](flip-board/) — тёмное механическое табло с четырьмя flip-панелями.
3. [Info Tiles](info-tiles/) — время, дата, заряд, погода, пульс и шаги.
4. [Codex Weekly](codex-weekly/) — крупные часы, недельный лимит Codex и heatmap личного
   использования.
5. [Starry Digits](starry-digits/) — рукописные бело-голубые цифры на живописном звёздном
   фоне с вихревыми масляными мазками.
6. [meded90](meded90/) — пиксельный портрет в красном пиджаке и вертикальные часы,
   размещённые в свободной области фона.
7. [Zodiac: Aquarius](zodiac-aquarius/) — иллюстрация Водолея и вертикальные часы.
8. [Zodiac: Gemini](zodiac-gemini/) — иллюстрация Близнецов и крупные часы.

## Нативные превью 200×228

| [Mosaic Grid](mosaic-grid/) | [Flip Board](flip-board/) | [Info Tiles](info-tiles/) | [Codex Weekly](codex-weekly/) |
| --- | --- | --- | --- |
| [![Mosaic Grid](assets/screenshots/mosaic-grid.png)](mosaic-grid/) | [![Flip Board](assets/screenshots/flip-board.png)](flip-board/) | [![Info Tiles](assets/screenshots/info-tiles.png)](info-tiles/) | [![Codex Weekly](assets/screenshots/codex-weekly.png)](codex-weekly/) |

| [Starry Digits](starry-digits/) | [meded90](meded90/) | [Zodiac: Aquarius](zodiac-aquarius/) | [Zodiac: Gemini](zodiac-gemini/) |
| --- | --- | --- | --- |
| [![Starry Digits](assets/screenshots/starry-digits.png)](starry-digits/) | [![meded90](assets/screenshots/meded90.png)](meded90/) | [![Zodiac: Aquarius](assets/screenshots/zodiac-aquarius.png)](zodiac-aquarius/) | [![Zodiac: Gemini](assets/screenshots/zodiac-gemini.png)](zodiac-gemini/) |

Каждая папка является отдельным Pebble-проектом со своим UUID и собирается в
отдельный `.pbw`. Это важно: в Appstore они публикуются как восемь отдельных
карточки.

## Быстрый старт

Установите Pebble CLI и SDK:

```bash
brew install node libpng uv
uv tool install pebble-tool
pebble sdk install latest
```

Соберите все восемь проектов:

```bash
make build
```

Отдельные watchapp-приложения собираются командой `make apps`. Для
`voice-drop` нужен специальный пропатченный SDK, описанный в его README.

Готовые пакеты появятся внутри:

```text
mosaic-grid/build/*.pbw
flip-board/build/*.pbw
info-tiles/build/*.pbw
codex-weekly/build/*.pbw
starry-digits/build/*.pbw
meded90/build/*.pbw
zodiac-aquarius/build/*.pbw
zodiac-gemini/build/*.pbw
```

## Локальная разработка и тестирование

Встроенного веб-сервера у Pebble-проектов нет. Роль dev server выполняет
эмулятор из Pebble SDK: CLI собирает watchface, запускает Pebble Time 2
(`emery`) и устанавливает в него свежий `.pbw`.

Дополнительно, как альтернативу SDK-эмулятору для тестирования, можно использовать
[pebble-qemu-wasm](https://github.com/ericmigi/pebble-qemu-wasm) — эмулятор Pebble
на WebAssembly с выводом экрана прямо в браузере. Доступно
[онлайн-демо](https://ericmigi.github.io/pebble-qemu-wasm/), а инструкции по
локальному запуску находятся в README проекта. Такой вариант удобен для
визуальной и интерактивной проверки, в том числе через плагин «Браузер» в Codex:
можно видеть результат, нажимать кнопки часов мышью или клавиатурой,
прокликивать меню и проверять корректность поведения интерфейса.
Это дополнительный способ проверки, не заменяющий финальный тест на часах.

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
любую другую папку циферблата и выполните те же две команды.

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
dist/zodiac-aquarius.pbw
dist/zodiac-gemini.pbw
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

В каждом `package.json` проверьте поле `author`. UUID уже уникальны — после
публикации их нельзя менять, иначе Appstore воспримет сборку как новое
приложение.

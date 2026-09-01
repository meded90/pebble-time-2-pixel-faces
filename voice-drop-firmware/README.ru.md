# PebbleOS Voice Drop patch

[English](README.md) · **Русский**

Патч рассчитан на актуальную ветку `main` репозитория Core Devices PebbleOS и
только на Emery / Pebble Time 2. Он добавляет:

- системный сервис записи микрофона в Speex 16 kHz, 9.8 kbps;
- PFS-очередь до 8 МБ с чанками по 16 КБ;
- принудительную фиксацию последнего чанка и метафайла сразу после остановки;
- очистку оборванных записей при загрузке;
- Pebble Protocol endpoint `10001` и pull/ACK передачу;
- сигнал наличия очереди каждые 30 минут;
- SDK API `voice_drop_start`, `voice_drop_stop`, `voice_drop_get_status`;
- SDK revision 110.

`voice_drop_stop()` сначала останавливает микрофон, затем закрывает все чанки в
PFS и создаёт метафайл-коммит последним. Пока операция не закончена, API
возвращает состояние `VoiceDropStateStopping`; запись не считается готовой и не
появляется в очереди синхронизации. Закрытие watchapp во время диктовки вызывает
тот же путь сохранения. Это не запускает внеплановую передачу на телефон:
синхронизация остаётся раз в 30 минут либо при новом подключении.

Сборка:

```bash
git clone https://github.com/coredevices/PebbleOS.git
cd PebbleOS
git apply /path/to/voice-drop-firmware/pebbleos.patch
python3 ./pbl configure --board obelix@pvt
python3 ./pbl build
```

Для другой аппаратной ревизии замените `pvt` на соответствующую ревизию Obelix.
После сборки используйте сгенерированный SDK `build/sdk/emery` для watchapp из
`../voice-drop`. Стандартный SDK 4.17 не содержит новых shim-функций и для этой
версии приложения не подходит.

Верификация патча выполнена на `qemu_emery`: `voice_drop.c` компилируется и
линкуется в `libservices_voice.a`, а генератор native SDK видит все три функции.
Полная локальная сборка qemu дальше остановилась на несовместимости установленного
Homebrew ARM toolchain со сборкой штатного demo-приложения (`stdint.h`), не в
коде Voice Drop.

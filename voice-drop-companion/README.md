# Voice Drop mobile bridge

Отдельное PebbleKit-приложение здесь намеренно не используется: оно не получает
сырые пакеты системного Pebble Protocol endpoint. Вместо этого
`coredevices-mobileapp.patch` добавляет общий Kotlin Multiplatform-модуль прямо
в открытое приложение Core Devices; один и тот же код работает на iOS и Android.

Модуль:

- слушает endpoint `10001`;
- при подключении и по 30-минутному сигналу часов начинает pull;
- подтверждает каждый блок смещением, поэтому повреждённый или переставленный
  блок не принимается;
- собирает не более 8 МБ, проверяет CRC32 и число Speex-кадров;
- вызывает только явно установленный `VoiceDropUploadSink`;
- отправляет часам ACK удаления лишь после `upload(...) == true`.

По умолчанию sink равен `null`: аудио никуда не отправляется, а запись остаётся
на часах. Приложение-хост должно явно установить
`VoiceDropUploadSinkRegistry.sink` после того, как пользователь ввёл точный
HTTPS URL и отдельный device token. Telegram bot token в мобильное приложение
не передаётся.

Применение к upstream:

```bash
git clone https://github.com/coredevices/mobileapp.git
cd mobileapp
git apply /path/to/voice-drop-companion/coredevices-mobileapp.patch
```

Ожидаемый payload для sink доступен как `VoiceDropRecording.toContainer()` и
принимается сервером `../voice-drop-server` с Content-Type
`application/x-voice-drop`.

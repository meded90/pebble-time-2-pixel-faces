# Voice Drop server

[English](README.md) · **Русский**

Сервер принимает контейнеры `application/x-voice-drop`, проверяет CRC32,
собирает исходный Speex/Ogg, перекодирует его в Ogg/Opus и вызывает Telegram
Bot API `sendVoice`. Ответ `2xx` возвращается только после подтверждения
Telegram; поэтому мобильный мост может безопасно подтвердить удаление записи с
часов.

## Настройка

1. Создайте Telegram-бота через BotFather и добавьте его в приватную группу или
   канал с правом публикации.
2. Скопируйте `.env.example` в защищённые переменные вашего хостинга.
3. Сгенерируйте отдельный длинный `VOICE_DROP_DEVICE_TOKENS`. Это не Telegram
   bot token: на телефоне должен храниться только этот ограниченный токен.
4. Публикуйте сервер только через HTTPS. Для Docker достаточно прокси Caddy,
   Nginx или Cloudflare Tunnel перед портом `8787`.

```bash
docker build -t voice-drop-server .
docker run --env-file .env -v voice-drop-data:/data -p 8787:8787 voice-drop-server
```

Проверка библиотек без Telegram и ffmpeg:

```bash
npm test
```

Контейнер идемпотентен по `recording_id`: повтор после потери ответа не создаёт
второе голосовое сообщение, если первое уже помечено доставленным.

# Voice Drop server

**English** · [Русский](README.ru.md)

The server accepts `application/x-voice-drop` containers, verifies CRC32,
reconstructs Speex/Ogg, transcodes it to Ogg/Opus, and calls the Telegram Bot API
`sendVoice`. It returns `2xx` only after Telegram confirms delivery, allowing the
mobile bridge to acknowledge deletion safely.

## Setup

1. Create a Telegram bot through BotFather and add it to a private group or
   channel with permission to post.
2. Copy `.env.example` into your hosting provider's protected variables.
3. Generate a separate long `VOICE_DROP_DEVICE_TOKENS` value. The phone stores
   this restricted token, never the Telegram bot token.
4. Publish the server only over HTTPS.

```bash
docker build -t voice-drop-server .
docker run --env-file .env -v voice-drop-data:/data -p 8787:8787 voice-drop-server
```

Run library tests without Telegram or ffmpeg:

```bash
npm test
```

The container is idempotent by `recording_id`, so retrying after a lost response
does not create a second voice message once the first is marked delivered.

# Codex Weekly

![Codex Weekly icon](../assets/icons/codex-weekly-icon-144.png)

Нативный watchface для Pebble Time 2 (`emery`, 200×228):

- очень крупное время;
- только самый длинный доступный лимит Codex;
- процент остатка и время до сброса;
- heatmap личного использования за 12 недель на всю ширину экрана;
- подписи месяцев и жёлтые границы перехода между месяцами;
- фоновые обновления через PebbleKit JS.

Квадрат в правом верхнем углу показывает состояние синхронизации: серый —
bridge не настроен, голубой — обновление выполняется, зелёный — данные получены,
красный — ошибка соединения или ответа.

## Запуск через Cloud Run

У проекта нет общего сервера, предустановленного URL или Google Cloud project
ID. Каждый пользователь разворачивает собственный экземпляр и вставляет в
настройки watchface два значения: URL `/status` и отдельный клиентский токен.

Пошаговые инструкции:

- [Русский](cloud-run/README.md)
- [English](cloud-run/README.en.md)

После успешной синхронизации квадрат становится зелёным. При отсутствии
данных, ошибке токена или временной ошибке сервиса watchface показывает `—` и
серые блоки — старые или выдуманные данные не выводятся.

## Локальный bridge (необязательная альтернатива)

Codex App Server предоставляет официальные методы
`account/rateLimits/read` и `account/usage/read`. Они используют существующий
вход ChatGPT в Codex на Mac. Обычный OpenAI Platform API key не подходит для
этого watchface: Platform usage и персональные лимиты ChatGPT/Codex — разные
системы.

Локальный bridge запускается на Mac, запрашивает только лимиты и дневные счётчики,
преобразует их в компактный JSON и отдаёт PebbleKit JS. Токен bridge хранится
на телефоне и не отправляется на часы.

Cloud Run использует этот же App Server, но хранит копию авторизации Codex в
Secret Manager. Это экспериментальный, не поддерживаемый OAuth-контракт: после
`codex logout`, нового `codex login` или `401` нужно загрузить новую версию
секрета по [инструкции Cloud Run](cloud-run/README.md).

## Где взять ключ

Для личных лимитов Codex нужен вход через ChatGPT, а не OpenAI Platform API
key. Официальная инструкция: [Codex authentication](https://learn.chatgpt.com/docs/auth.md).

1. Авторизуйте Codex на Mac:

   ```bash
   codex login
   ```

2. Проверьте статус входа:

   ```bash
   codex login status
   ```

3. Создайте отдельный секрет для локального bridge:

   ```bash
   openssl rand -hex 24
   ```

Полученная строка и есть `CODEX_PEBBLE_TOKEN`. Её не нужно получать на сайте:
она создаётся локально и защищает только ваш bridge. Не вставляйте в настройки
watchface ключ со страницы [OpenAI Platform API keys](https://platform.openai.com/api-keys) —
API-key-only авторизация не предоставляет персональные лимиты и историю
использования Codex.

## Запуск bridge

Сначала выполните шаги из раздела «Где взять ключ».

Запустите bridge в локальной сети, подставив созданный токен:

```bash
CODEX_PEBBLE_HOST=0.0.0.0 \
CODEX_PEBBLE_TOKEN=replace-with-generated-token \
node bridge/server.mjs
```

При запуске bridge сам выполнит эквивалент команды
`ipconfig getifaddr en0`, проверит `/healthz` и выведет готовый адрес:

```text
Network: ipconfig getifaddr en0 -> 192.168.1.20
Status URL: http://192.168.1.20:8765/status
Health check: OK (http://192.168.1.20:8765/healthz)
```

Если активная сеть использует другой интерфейс, укажите его при запуске,
например `CODEX_PEBBLE_INTERFACE=en1`.

В настройках watchface укажите:

```text
Status URL: http://IP-АДРЕС-MAC:8765/status
Bridge token: токен из CODEX_PEBBLE_TOKEN
```

Телефон и Mac должны находиться в одной сети, а Mac должен быть включён.
Не публикуйте HTTP-порт bridge в интернете. Для удалённого доступа используйте
HTTPS reverse proxy или защищённый tunnel.

Проверка bridge с Mac:

```bash
curl \
  -H "Authorization: Bearer replace-with-generated-token" \
  http://127.0.0.1:8765/status
```

## Сборка

```bash
npm install
pebble build
pebble install --emulator emery
```

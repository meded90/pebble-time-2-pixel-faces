# Wrist Agent Server

[Русский](README.md) · [English](README.en.md)

Приватный HTTPS bridge между PebbleKit JS и опубликованным ChatGPT Workspace
Agent. Он запускает агента, отслеживает только статус run и принимает короткий
ответ через MCP tool `send_to_pebble`.

Такой callback обязателен: Workspace Agents API сейчас возвращает `202`, URL
разговора и опциональный beta run ID, но не позволяет получить текст ответа.
См. [официальный trigger API](https://developers.openai.com/workspace-agents/trigger-runs).

## Поток данных

```text
Pebble Dictation -> PebbleKit JS -> POST /v1/requests
                                      |
                                      v
                         Workspace Agent API channel
                                      |
                         apps / connectors / web / MCP
                                      |
                  send_to_pebble(request capability)
                                      |
                         GET /v1/requests/:id -> часы
```

OpenAI-токен хранится только на сервере. Телефон знает URL bridge и отдельный
device token. `/mcp` принимает одноразовую capability конкретного запроса.

## 1. Подготовить Workspace Agent

1. Администратор workspace включает Workspace Agents и выдачу access tokens.
2. Создайте агента и подключите только необходимые apps/connectors. Для начала
   безопаснее дать календарю и файлам read-only доступ.
3. Добавьте текст из [`AGENT_INSTRUCTIONS.md`](AGENT_INSTRUCTIONS.md) в
   инструкции агента.
4. Опубликуйте API channel и сохраните стабильный ID вида `agtch_...`.
5. Создайте Workspace Agent access token с нужным scope. Это не обычный OpenAI
   API key. Подробности: [официальная authentication guide](https://developers.openai.com/workspace-agents/authentication).

## 2. Создать конфигурацию

```bash
cd wrist-agent-server
cp .env.example .env
openssl rand -base64 32  # device token
openssl rand -base64 48  # independent capability pepper
```

Заполните `.env`:

```dotenv
PUBLIC_BASE_URL=https://agent.example.com
WRIST_AGENT_DEVICE_TOKENS=<случайный device token>
CALLBACK_CAPABILITY_PEPPER=<другая случайная строка>
WORKSPACE_AGENT_TRIGGER_ID=agtch_...
WORKSPACE_AGENT_ACCESS_TOKEN=<workspace agent access token>
```

Значения из `.env.example` намеренно считаются невалидными: server завершится
с ошибкой, пока каждый placeholder не заменён реальным секретом или URL.

Не кладите секреты в URL, PBW, Git или настройки агента.

## 3. Проверить локально

```bash
npm ci
npm run check
npm test
```

Тесты поднимают loopback HTTP server, выполняют настоящий Streamable HTTP MCP
handshake, проверяют idempotency, callback capability и двухэтапное подтверждение.

Запуск для разработки:

```bash
PUBLIC_BASE_URL=http://localhost:8787 npm start
curl http://localhost:8787/healthz
npx @modelcontextprotocol/inspector@latest
```

В Inspector выберите Streamable HTTP и URL `http://localhost:8787/mcp`.
`npm start` загружает остальные значения из локального `.env` через встроенный
Node `--env-file`.

## 4. Развернуть

Нужен стабильный публичный HTTPS URL. Самый простой вариант — один Docker
instance с persistent volume:

```bash
docker compose up --build -d
```

Compose публикует порт `8787` только на `127.0.0.1`; поставьте на том же host
HTTPS reverse proxy. Для proxy в другом контейнере замените host-port на общую
private Docker network и не открывайте plaintext port наружу. Volume `/app/data`
обязателен для восстановления идемпотентных
запросов после рестарта. Не запускайте несколько replicas с файловым store;
для горизонтального масштабирования нужен PostgreSQL/managed database.

После deployment:

```bash
curl https://agent.example.com/healthz
npx @modelcontextprotocol/inspector@latest
```

Повторно проверьте `https://agent.example.com/mcp` через Inspector.
Bridge также ограничивает частоту `/mcp`; лимит настраивается через
`MCP_RATE_LIMIT_PER_MINUTE`.

## 5. Подключить MCP к агенту

1. В ChatGPT включите Developer mode, если это разрешено политикой workspace.
2. Создайте private plugin/connection с URL
   `https://agent.example.com/mcp`.
3. Проверьте, что найден tool `send_to_pebble` и его annotations.
4. Добавьте plugin к Workspace Agent, затем перепубликуйте API channel.
5. Выполните тестовый read-only запрос с часов и убедитесь, что callback вернул
   ответ. После этого отдельно проверьте сценарий `needs_confirmation`.

Официальный порядок подключения: [Connect and test your plugin](https://developers.openai.com/plugins/deploy/connect-chatgpt).
Для public OpenAI plugin требуется другой auth boundary с OAuth 2.1; см.
[MCP authentication](https://developers.openai.com/plugins/build/auth).

## API телефона

### `POST /v1/requests`

```http
Authorization: Bearer <device token>
Idempotency-Key: pebble-...
Content-Type: application/json
```

```json
{
  "command": "Создай напоминание завтра в 15:00",
  "utcOffsetMinutes": 240,
  "source": "pebble-time-2"
}
```

### `GET /v1/requests/:id`

Возвращает `queued`, `in_progress`, `needs_attention`, `completed`, `failed`
или `expired`, короткий ответ и action summary. Upstream `suspended` нельзя
возобновить через документированный API: часы просят открыть conversation в
ChatGPT.

### `POST /v1/requests/:id/decision`

```json
{ "decision": "approve" }
```

или `reject`. Обе операции требуют нового `Idempotency-Key`.

## Ограничения

- Voice audio обрабатывает настроенная Pebble/Core Devices dictation-служба;
  watchapp получает только принятую транскрипцию.
- Ответ зависит от доступности телефона, bridge, Workspace Agent и connectors.
- Live E2E нельзя доказать без реального опубликованного `agtch_...`, workspace
  access token и публичного HTTPS MCP endpoint.
- Политика двухэтапного подтверждения описана в [`SECURITY.md`](SECURITY.md).

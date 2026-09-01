# Wrist Agent on Google Cloud Functions Gen 2

[English](GCP_FUNCTIONS.en.md) · [Основной README](README.md)

Этот вариант развёртывания предназначен для production bridge без собственного
сервера. Cloud Functions Gen 2 запускает HTTP entry point
`wristAgentBridge`, Firestore хранит запросы и идемпотентность между cold
start/несколькими экземплярами, а Secret Manager хранит все три секрета.

## Что делает deploy-скрипт

Один запуск `scripts/deploy-google-cloud-function.sh`:

1. включает нужные Google Cloud API;
2. создаёт default Firestore Native database с delete protection — только после
   явного `--create-firestore`;
3. создаёт отдельный runtime service account с `roles/datastore.user` и
   помечает его как ресурс Wrist Agent;
4. выдаёт выбранному Cloud Build identity роль builder и право временно
   присоединять только этот runtime account;
5. создаёт или переиспользует помеченные Secret Manager secrets для Workspace
   Agent token, Pebble device token и callback pepper;
6. выдаёт runtime account доступ к этим трём секретам;
7. включает Firestore TTL на private request/idempotency collections;
8. развёртывает public HTTP Function Gen 2 на Node.js 22, проверяет
   `/healthz` и `/readyz`, затем печатает bridge URL и MCP URL.

Firestore location нельзя изменить после создания. Поэтому скрипт не создаёт
базу молча: сначала выберите регион и локацию Firestore в
[официальном списке](https://cloud.google.com/firestore/docs/locations).
Для production рекомендуется отдельный GCP project: скрипт создаёт IAM
bindings, но не может доказать отсутствие старых унаследованных ролей у
ресурса, который вы явно забрали через `--force-takeover`.

## Перед первым запуском

1. Установите актуальный [Google Cloud CLI](https://cloud.google.com/sdk/docs/install)
   и авторизуйтесь в нужном проекте. В проекте должен быть включён billing.
2. У вашей активной gcloud identity должны быть разрешения включать API,
   создавать Firestore/секреты/service account, выдавать роли и deploy
   Cloud Functions. Для самого deploy identity также нужен `Service Account
   User` на созданном runtime account. При корпоративных IAM-политиках это обычно делает
   администратор.
3. В ChatGPT Workspace Agent заранее создайте и опубликуйте API channel
   `agtch_...`; именно его ID передаётся скрипту.
4. Выполните локальные проверки:

```bash
cd wrist-agent-server
npm ci
npm run check
npm test
```

## Одна команда deployment

Скрипт скрыто запросит Workspace Agent access token в терминале. Он не
попадает в URL, исходники или shell history.

```bash
cd wrist-agent-server

./scripts/deploy-google-cloud-function.sh \
  --project YOUR_GCP_PROJECT \
  --region europe-west1 \
  --firestore-location eur3 \
  --create-firestore \
  --workspace-agent-trigger-id agtch_YOUR_PUBLISHED_CHANNEL \
  --print-device-token
```

Для CI/неинтерактивного запуска передайте Workspace secret через защищённый
механизм CI как environment variable `WORKSPACE_AGENT_ACCESS_TOKEN`, добавьте
`--non-interactive`, либо передайте путь к файлу с правами `0600`. Device token
передавайте в защищённый CI output/file, а не в build log:

```bash
./scripts/deploy-google-cloud-function.sh \
  --project YOUR_GCP_PROJECT \
  --region europe-west1 \
  --firestore-location eur3 \
  --create-firestore \
  --workspace-agent-trigger-id agtch_YOUR_PUBLISHED_CHANNEL \
  --workspace-agent-token-file /secure/path/workspace-agent-token.txt \
  --device-token-output-file /secure/output/wrist-agent-device-token.txt \
  --non-interactive
```

Скрипт выводит:

```text
BRIDGE_URL=https://REGION-PROJECT.cloudfunctions.net/wrist-agent-bridge
MCP_URL=https://REGION-PROJECT.cloudfunctions.net/wrist-agent-bridge/mcp
DEVICE_TOKEN=...
```

`DEVICE_TOKEN` нужен один раз для настройки Pebble. Скрипт показывает его
только при явном `--print-device-token` в интерактивном терминале. В CI
`--device-token-output-file` создаёт файл с mode `0600`; заберите его
защищённым механизмом CI и сохраните token в менеджере секретов. Workspace
access token и callback pepper скрипт никогда не печатает.

## После deployment — обязательный ручной шаг в ChatGPT

Google Cloud не имеет права менять настройки ChatGPT Workspace за владельца
workspace. Поэтому:

1. в ChatGPT включите Developer mode, если это разрешено;
2. создайте private MCP connection с `MCP_URL`;
3. прикрепите connection к Workspace Agent;
4. перепубликуйте API channel;
5. в Pebble config введите `BRIDGE_URL` и `DEVICE_TOKEN`;
6. проверьте один read-only запрос, затем отдельно сценарий confirmation.

Полный порядок подключения агента: [OpenAI Connect and test your
plugin](https://developers.openai.com/plugins/deploy/connect-chatgpt).

## Повторный deploy и ротация

Обычный повторный deploy не создаёт новые версии секретов; он переиспользует
текущие pinned версии:

```bash
npm run deploy:gcf -- \
  --project YOUR_GCP_PROJECT \
  --region europe-west1 \
  --workspace-agent-trigger-id agtch_YOUR_PUBLISHED_CHANNEL
```

Не передавайте `--firestore-location`, если хотите принять существующую
локацию базы; `--create-firestore` тоже больше не нужен.

При повторном запуске script намеренно не печатает device token в терминал или
CI log, даже если он был только что сгенерирован. Для локального запуска
явно добавьте `--print-device-token`; для CI используйте
`--device-token-output-file`.

Для явной ротации:

```bash
# Требует новый token file; новая function revision привязывается к новой версии.
./scripts/deploy-google-cloud-function.sh ... \
  --workspace-agent-token-file /secure/path/new-token.txt \
  --rotate-workspace-token

# Сгенерирует новый device token. Сразу обновите Pebble config.
./scripts/deploy-google-cloud-function.sh ... \
  --rotate-device-token --print-device-token

# Инвалидирует незавершённые callback capabilities; используйте только осознанно.
./scripts/deploy-google-cloud-function.sh ... --rotate-callback-pepper
```

TTL в Firestore включается асинхронно; Google указывает, что активация может
занять не менее 10 минут, а удаление истёкших документов выполняется
асинхронно. Bridge дополнительно не выдаёт истёкшие запросы, а срок retention
по умолчанию — 24 часа.

## Границы безопасности и эксплуатации

- Public HTTP access нужен намеренно: Pebble phone companion и ChatGPT MCP не
  могут предъявить Google IAM identity. `/v1` всё равно требует отдельный
  device token, а `/mcp` — одноразовую request capability.
- Нет JSON service-account keys, `.env` или Firestore credentials в
  deployment source. Function использует Application Default Credentials
  runtime service account.
- Firestore transaction + trigger lease предотвращают конкурентный запуск
  одного upstream request на разных function instances. OpenAI idempotency key
  остаётся второй линией защиты.
- `--max-instances=4` ограничивает параллельность и стоимость; изменяйте
  только после нагрузочного теста.
- Если именованный function, runtime account или secret уже существует, но не
  помечен `managed-by=wrist-agent`, скрипт останавливается. Используйте
  `--force-takeover` только после проверки чужих IAM-ролей и secret versions;
  этот флаг добавляет маркер владения для следующих безопасных запусков.
- Скрипт не делает git commit, push и не меняет ChatGPT Workspace. Он
  изменяет только явно указанный GCP project.
- Organization policy может запретить `--allow-unauthenticated`. Это
  намеренно завершается ошибкой: не обходите политику, настройте одобренный
  ingress/auth gateway и скорректируйте архитектуру.

## Диагностика

```bash
gcloud functions describe wrist-agent-bridge \
  --v2 --project YOUR_GCP_PROJECT --region europe-west1

gcloud functions logs read wrist-agent-bridge \
  --gen2 --project YOUR_GCP_PROJECT --region europe-west1 \
  --limit=100

curl -fsS https://REGION-PROJECT.cloudfunctions.net/wrist-agent-bridge/healthz
curl -fsS https://REGION-PROJECT.cloudfunctions.net/wrist-agent-bridge/readyz
```

Если `/healthz` проходит, а `/readyz` нет, проверьте существование
Firestore Native database, роль `roles/datastore.user` runtime service
account и журналы cold start. Если получаете `401` от Workspace Agent,
безопасно ротируйте только Workspace Agent token: не копируйте личные
Codex/ChatGPT auth-файлы в Secret Manager.

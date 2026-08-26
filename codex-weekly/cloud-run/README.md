# Codex Weekly в Google Cloud Run

[English version](README.en.md)

Это пошаговая инструкция для развёртывания **собственного** backend-сервиса
Codex Weekly. В репозитории нет общего Google Cloud project ID, адреса сервиса,
имени пользователя или готового секрета. Все облачные ресурсы создаются в
вашем проекте Google Cloud.

Технически это Cloud Run service, а не Cloud Functions: контейнер запускает
Codex CLI и общается с `codex app-server` через stdio.

## Что получится

```text
PebbleKit JS ── HTTPS + клиентский токен ──> ваш Cloud Run service
                                                   │
                                                   ├─ Secret Manager: клиентский токен
                                                   ├─ Secret Manager: копия Codex auth
                                                   └─ Codex App Server: account/rateLimits/read
```

- `GET /health` публично возвращает состояние контейнера.
- `GET /status` требует `Authorization: Bearer <token>` и возвращает только
  нормализованное окно лимита `limitId: "codex"`.
- При `503` endpoint добавляет безопасный код: `AUTH` (вход Codex истёк),
  `TIME` (таймаут), `DATA` (нет основного лимита) или `ERR`. Секреты и
  подробный текст ошибки наружу не передаются.
- Секреты, история чатов и произвольные методы App Server наружу не выдаются.
- Cloud Run масштабируется до нуля и ограничен одним экземпляром.

## Важное ограничение авторизации

Метод `account/rateLimits/read` и вход Codex через ChatGPT документированы
OpenAI. Однако копирование локального `auth.json` в Secret Manager не является
поддерживаемым облачным OAuth-контрактом. Это добровольный эксперимент:

- секрет даёт доступ к подключённому аккаунту ChatGPT;
- после `codex logout`, нового входа, ротации токена, ответа `401` или
  несовместимого обновления Codex секрет потребуется загрузить заново;
- OpenAI может изменить внутренний формат файла;
- для продукта с несколькими пользователями нужен отдельный поддерживаемый
  механизм авторизации, а не этот скрипт.

Если такой риск неприемлем, используйте локальный
[bridge](../bridge/server.mjs), который работает с обычным локальным входом
Codex и не копирует учётные данные в облако.

Официальные источники:

- [Codex authentication](https://learn.chatgpt.com/docs/auth)
- [Codex App Server](https://learn.chatgpt.com/docs/app-server)
- [Cloud Run: развёртывание из исходного кода](https://cloud.google.com/run/docs/deploying-source-code)
- [Secret Manager: добавление версии секрета](https://cloud.google.com/secret-manager/docs/add-secret-version)
- [Secret Manager: доступ к версии секрета](https://cloud.google.com/secret-manager/docs/access-secret-version)

## 1. Установите необходимые инструменты

Понадобятся:

- macOS или Linux с Bash;
- [Google Cloud CLI](https://cloud.google.com/sdk/docs/install);
- [Codex CLI](https://learn.chatgpt.com/docs/codex/cli);
- `openssl`, `curl` и Git;
- аккаунт Google Cloud с billing account;
- подписка/рабочая область ChatGPT, в которой доступен Codex.

Проверьте команды:

```bash
gcloud --version
codex --version
openssl version
curl --version
```

Все следующие команды выполняются из корня этого репозитория.

## 2. Выберите собственные параметры

Google Cloud project ID должен быть глобально уникальным. Замените пример на
свой ID; не используйте адрес электронной почты или полное имя.

```bash
export GCP_PROJECT_ID="your-unique-codex-weekly-project"
export CLOUD_RUN_REGION="us-central1"
export CLOUD_RUN_SERVICE_NAME="codex-weekly"
export CLOUD_RUN_SERVICE_ACCOUNT_NAME="codex-weekly-run"
export CLOUD_BUILD_SERVICE_ACCOUNT_NAME="codex-weekly-build"
```

Скрипты намеренно не задают project ID по умолчанию. Не закрывайте терминал до
окончания настройки: переменные нужны на следующих шагах.

## 3. Создайте и настройте Google Cloud project

Авторизуйтесь:

```bash
gcloud auth login
gcloud auth list
```

Создайте новый проект или убедитесь, что выбранный проект уже существует:

```bash
gcloud projects create "$GCP_PROJECT_ID" --name="Codex Weekly"
gcloud config set project "$GCP_PROJECT_ID"
```

Если команда сообщает, что проект уже существует, не создавайте его повторно.
Найдите billing account и подключите его:

```bash
gcloud billing accounts list
export GCP_BILLING_ACCOUNT_ID="replace-with-your-billing-account-id"
gcloud billing projects link "$GCP_PROJECT_ID" \
  --billing-account="$GCP_BILLING_ACCOUNT_ID"
```

Создайте budget alert в Google Cloud Console перед развёртыванием. Бесплатные
лимиты и цены могут меняться, поэтому инструкция не предполагает нулевую
стоимость.

## 4. Войдите в Codex через ChatGPT

Для персональных лимитов Codex нужен вход через ChatGPT. Авторизация только
OpenAI API key не предоставляет эти данные App Server.

```bash
codex login
codex login status
```

Завершите браузерный вход и убедитесь, что `codex login status` успешен.

## 5. Загрузите копию Codex auth в Secret Manager

Прочитайте раздел об ограничениях выше. Скрипт требует явного флага
подтверждения, читает локальный файл авторизации и передаёт его напрямую в
Secret Manager. Содержимое не печатается и не сохраняется во временный файл.

```bash
GCP_PROJECT_ID="$GCP_PROJECT_ID" \
  codex-weekly/cloud-run/sync-codex-auth-to-gcp.sh \
  --confirm-copy-codex-auth
```

По умолчанию скрипт читает `${CODEX_HOME}/auth.json`, если задан `CODEX_HOME`,
иначе `~/.codex/auth.json`. Для нестандартного пути укажите:

```bash
export CODEX_AUTH_FILE="/absolute/path/to/auth.json"
```

Скрипт создаёт:

- service account `codex-weekly-run` (или выбранное вами имя);
- секрет `codex-weekly-codex-auth`;
- новую версию секрета с текущей авторизацией.

## 6. Разверните Cloud Run service

```bash
GCP_PROJECT_ID="$GCP_PROJECT_ID" \
CLOUD_RUN_REGION="$CLOUD_RUN_REGION" \
CLOUD_RUN_SERVICE_NAME="$CLOUD_RUN_SERVICE_NAME" \
CLOUD_RUN_SERVICE_ACCOUNT_NAME="$CLOUD_RUN_SERVICE_ACCOUNT_NAME" \
CLOUD_BUILD_SERVICE_ACCOUNT_NAME="$CLOUD_BUILD_SERVICE_ACCOUNT_NAME" \
  codex-weekly/cloud-run/deploy.sh
```

Скрипт:

1. проверяет выбранный проект и активный вход `gcloud`;
2. включает необходимые Google Cloud APIs;
3. создаёт отдельные runtime и build service accounts, если их ещё нет;
4. создаёт случайный клиентский токен в `codex-weekly-client-token`;
5. выдаёт runtime service account доступ только к двум нужным секретам;
6. собирает контейнер из `codex-weekly/cloud-run`;
7. разворачивает Cloud Run с `min-instances=0`, `max-instances=1` и
   `concurrency=1`;
8. выводит URL `/status`, но не значение токена.

Флаг `--allow-unauthenticated` относится к Google IAM: PebbleKit JS не может
предъявить Google identity. Сам `/status` всё равно защищён отдельным bearer
token, который обработчик сравнивает постоянным по времени способом.

## 7. Получите URL и клиентский токен

```bash
export CODEX_WEEKLY_SERVICE_URL="$(gcloud run services describe \
  "$CLOUD_RUN_SERVICE_NAME" \
  --project="$GCP_PROJECT_ID" \
  --region="$CLOUD_RUN_REGION" \
  --format='value(status.url)')"

export CODEX_WEEKLY_CLIENT_TOKEN="$(gcloud secrets versions access latest \
  --secret=codex-weekly-client-token \
  --project="$GCP_PROJECT_ID")"
```

Не печатайте токен, не добавляйте его в Git, URL, скриншоты, логи или чаты.
Переменная нужна только для проверки и вставки в настройки PebbleKit.

## 8. Проверьте сервис

```bash
curl -i "$CODEX_WEEKLY_SERVICE_URL/health"
curl -i "$CODEX_WEEKLY_SERVICE_URL/status"
curl -i \
  -H "Authorization: Bearer $CODEX_WEEKLY_CLIENT_TOKEN" \
  "$CODEX_WEEKLY_SERVICE_URL/status"
```

Ожидается:

- `/health` → `200` и `{"ok":true}`;
- `/status` без токена → `401`;
- `/status` с токеном → `200` и JSON с `weekly.leftPercent`,
  `weekly.windowDurationMins` и `weekly.resetsAt`.

Если последний запрос возвращает `503`, посмотрите логи:

```bash
gcloud run services logs read "$CLOUD_RUN_SERVICE_NAME" \
  --project="$GCP_PROJECT_ID" \
  --region="$CLOUD_RUN_REGION" \
  --limit=100
```

Watchface покажет этот безопасный код рядом с квадратом синхронизации и очистит
устаревшие значения. `AUTH` означает: снова выполните `codex login`, загрузите
новую версию auth-секрета шагом 5 и выпустите новую ревизию шагом 6.

## 9. Подключите watchface

В настройках Codex Weekly на телефоне укажите:

```text
Status URL: значение CODEX_WEEKLY_SERVICE_URL + /status
Cloud Run client token: значение CODEX_WEEKLY_CLIENT_TOKEN
```

Нажмите **Save and sync**. Зелёный индикатор означает успешную синхронизацию.
Красный индикатор означает ошибку URL, токена, авторизации Codex или сервиса.

После настройки удалите токен из текущего shell:

```bash
unset CODEX_WEEKLY_CLIENT_TOKEN
```

## Обновление и восстановление

После изменения кода повторно запустите `deploy.sh` с теми же переменными.
После нового `codex login`, `codex logout`, `401` или истечения авторизации
снова выполните шаг 5, а затем разверните новую ревизию шагом 6, чтобы новый
экземпляр гарантированно перечитал секрет.

Если нужно сменить клиентский токен, добавьте новую версию секрета и снова
разверните сервис:

```bash
openssl rand -base64 48 | tr -d '\n' | \
  gcloud secrets versions add codex-weekly-client-token \
    --project="$GCP_PROJECT_ID" \
    --data-file=-

GCP_PROJECT_ID="$GCP_PROJECT_ID" \
CLOUD_RUN_REGION="$CLOUD_RUN_REGION" \
  codex-weekly/cloud-run/deploy.sh
```

После ротации обновите токен в настройках watchface.

## Удаление ресурсов

Удалите только ресурсы, созданные для Codex Weekly:

```bash
gcloud run services delete "$CLOUD_RUN_SERVICE_NAME" \
  --project="$GCP_PROJECT_ID" \
  --region="$CLOUD_RUN_REGION"

gcloud secrets delete codex-weekly-client-token --project="$GCP_PROJECT_ID"
gcloud secrets delete codex-weekly-codex-auth --project="$GCP_PROJECT_ID"

gcloud iam service-accounts delete \
  "${CLOUD_RUN_SERVICE_ACCOUNT_NAME}@${GCP_PROJECT_ID}.iam.gserviceaccount.com" \
  --project="$GCP_PROJECT_ID"

gcloud iam service-accounts delete \
  "${CLOUD_BUILD_SERVICE_ACCOUNT_NAME}@${GCP_PROJECT_ID}.iam.gserviceaccount.com" \
  --project="$GCP_PROJECT_ID"
```

Удаляйте весь проект только если уверены, что в нём нет других ресурсов:

```bash
gcloud projects delete "$GCP_PROJECT_ID"
```

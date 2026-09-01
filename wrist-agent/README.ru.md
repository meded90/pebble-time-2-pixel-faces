# Wrist Agent

[English](README.md) · **Русский** · [Все проекты](../README.ru.md#галерея-проектов)

`Wrist Agent` — голосовой frontend для ChatGPT Workspace Agent на Pebble Time 2.
Часы запускают системную Dictation, показывают распознанную команду перед
отправкой, передают только принятый текст через телефон на ваш private bridge и
возвращают короткий ответ или итог действия.

Это не автономный ChatGPT-клиент и не raw-audio recorder. Workspace/OpenAI
credential никогда не хранится в PBW, часах или Clay.

## Что умеет

- задавать агенту вопросы голосом на языке настроенной dictation-службы;
- запускать опубликованный Workspace Agent с теми apps/connectors/MCP tools,
  которые разрешил владелец workspace;
- показывать короткий ответ и action summary с прокруткой кнопками Up/Down;
- восстанавливать незавершённый запрос после перезапуска PebbleKit JS;
- безопасно повторять HTTP после timeout с тем же `Idempotency-Key`;
- запрашивать отдельное подтверждение на часах перед предложенным write-action;
- показывать просьбу открыть ChatGPT, если upstream run перешёл в `suspended`.

## Требования

- Pebble Time 2 (`emery`, 200×228) и PebbleOS 4.32+;
- совместимое Pebble/Core Devices приложение на телефоне с рабочей Dictation;
- Bluetooth и интернет на телефоне;
- собственный HTTPS deployment из [`../wrist-agent-server`](../wrist-agent-server/);
- опубликованный ChatGPT Workspace Agent API channel (`agtch_...`) и Workspace
  Agent access token на сервере;
- private MCP connection `https://YOUR-BRIDGE/mcp`, добавленный к агенту.

Подробная серверная и agent-настройка: [wrist-agent-server/README.md](../wrist-agent-server/README.md).

## Настроить часы

1. Разверните bridge и проверьте `/healthz`, `/mcp` и тестовый trigger.
2. Установите `build/wrist-agent.pbw` или версию из RePebble Store.
3. В Pebble/Core Devices откройте настройки Wrist Agent.
4. Вставьте base HTTPS URL bridge, без `/v1/requests`.
5. Вставьте отдельный device token из `WRIST_AGENT_DEVICE_TOKENS`.
6. Сохраните. На экране появится `WRIST AGENT / SELECT SPEAK`.

Не вставляйте на телефоне `WORKSPACE_AGENT_ACCESS_TOKEN`: он принадлежит только
secret manager сервера.

## Управление

| Экран | Select | Up / Down | Back |
| --- | --- | --- | --- |
| Ready | Начать Dictation | Прокрутка | Выйти |
| System Dictation | Принять распознанный текст | Системное управление | Отмена |
| Send request? | Отправить точную команду | Просмотреть весь текст | Отменить |
| Agent working | Обновить статус | Прокрутка | Выйти; polling восстановится |
| Action needed | Подтвердить предложенное действие | Просмотреть summary | Отклонить |
| Done | Новый голосовой запрос | Прокрутка | Выйти |
| Phone/server error | Повторить тот же request либо начать новый — по подсказке | Прокрутка | Выйти |

Для write-action агент сначала должен вернуть `needs_confirmation`. После
Select bridge запускает второй этап в том же conversation. Если ChatGPT сам
требует host-confirmation, часы показывают просьбу открыть conversation.

## Данные и приватность

1. Audio обрабатывает настроенная Pebble/Core Devices dictation-служба.
2. Watchapp получает только принятую транскрипцию.
3. PebbleKit JS хранит URL, device token и минимальное состояние текущего
   запроса. Pending transcript хранится только до принятого ответа bridge.
4. Bridge передаёт команду в ChatGPT workspace и подключённые инструменты.
5. Короткий результат временно хранится на вашем bridge; стандартный retention
   — 24 часа.

У приложения нет developer-operated сервиса, telemetry, рекламы или analytics.
Фактическая обработка connected-app data определяется вашим ChatGPT workspace и
настройками bridge. Подробности: [`../wrist-agent-server/SECURITY.md`](../wrist-agent-server/SECURITY.md).

## Сборка

```bash
cd wrist-agent
npm ci
npm test
../.venv/bin/pebble clean
../.venv/bin/pebble build
unzip -t build/wrist-agent.pbw
```

Готовый пакет: `build/wrist-agent.pbw`. Целевая платформа — только `emery`.

## Проверка в emulator

```bash
../.venv/bin/pebble install --emulator emery
../.venv/bin/pebble transcribe --emulator emery "Создай тестовое напоминание"
# В другом терминале нажмите Select, затем примите системную транскрипцию.
../.venv/bin/pebble emu-button --emulator emery click select
../.venv/bin/pebble screenshot --emulator emery wrist-agent.png
```

Проверка ошибок Dictation:

```bash
../.venv/bin/pebble transcribe --emulator emery --error connectivity
../.venv/bin/pebble transcribe --emulator emery --error disabled
../.venv/bin/pebble transcribe --emulator emery --error no-speech-detected
```

Emulator доказывает native UI, AppMessage и Dictation flow, но не доказывает
работу реального phone voice service или live Workspace Agent. Перед публикацией
выполните checklist из [`PUBLISHING.md`](PUBLISHING.md).

## Текущие ограничения OpenAI API

Workspace Agents trigger API не возвращает текст ответа; поэтому агент обязан
вызвать MCP callback. Если он его пропустил, часы не смогут восстановить ответ
из run API. Официальная документация:

- [Trigger workspace agent runs](https://developers.openai.com/workspace-agents/trigger-runs)
- [Workspace Agent authentication](https://developers.openai.com/workspace-agents/authentication)
- [Build an MCP server](https://developers.openai.com/plugins/build/mcp-server)

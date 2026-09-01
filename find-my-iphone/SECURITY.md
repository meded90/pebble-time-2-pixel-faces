# Security notes

## Threat model

Приложение минимизирует передачу данных: Apple credentials обрабатываются только PebbleKit JS на iPhone, запросы идут непосредственно на HTTPS-домены Apple, а часы получают только безопасное представление состояния.

Это не превращает решение в официальный или полностью безопасный Apple-клиент. Reverse API может измениться без предупреждения; повторные неудачные входы могут вызвать дополнительную проверку или временную блокировку Apple Account.

## Secrets lifecycle

1. Страница настройки возвращает пароль или 2FA-код в PebbleKit JS через локальную схему `pebblejs://close`.
2. Пароль используется для PBKDF2-SHA256 и SRP-доказательств M1/M2.
3. Поле формы, объект ответа и локальная переменная очищаются сразу после запуска/завершения операции.
4. В `localStorage` записываются только сессионные данные Apple и метаданные устройства.
5. Logout удаляет все ключи приложения.

JavaScript не гарантирует немедленное физическое обнуление строк в памяти, поэтому очистка уменьшает время жизни секрета, но не равна secure-memory primitive.

## Storage keys

- `find_my_iphone_session_v1` — Apple session, cookies и сервис Find My;
- `find_my_iphone_pending_auth_v1` — только заголовки незавершённого 2FA challenge;
- `find_my_iphone_devices_v1` — ID и короткие имена iPhone;
- `find_my_iphone_preferences_v1` — выбранный device ID.

Хранилище UUID-scoped, но не Keychain. Не используйте приложение на чужом, jailbroken или недоверенном iPhone.

## Logging

Код не логирует Apple ID, пароль, 2FA, cookies, токены или полный ответ Apple. В watch AppMessage не включаются credentials. Ошибки нормализуются до коротких кодов.

## Dependency note

Криптография собирается локально из `bn.js`, `crypto-js` и `es6-promise`. `crypto-js` 4.2.0 больше активно не развивается; версия зафиксирована lock-файлом и используется только для SHA-256/PBKDF2/кодирования. Перед публикацией приложения зависимости нужно повторно проверить или заменить на поддерживаемую ES5-совместимую реализацию.

Если runtime предоставляет `crypto.getRandomValues`, SRP использует его. Bare JavaScriptCore на iOS может не предоставлять Web Crypto; тогда seed одноразового SRP exponent выводится SHA-256 из Apple challenge/cookies, Pebble account token, времени и jitter. Приложение никогда не использует один `Math.random()` как готовый криптографический ключ. Этот fallback требует отдельного аудита перед публичным релизом.

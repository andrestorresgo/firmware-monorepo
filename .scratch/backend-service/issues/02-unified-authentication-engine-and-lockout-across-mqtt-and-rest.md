# 02: Unified Authentication Engine and Lockout across MQTT and REST

**What to build:** The complete end-to-end authentication domain engine and lockout enforcement per ADR-0005. The service validates numeric User ID and plaintext PIN pairs originating from either the physical Gateway keypad (received via HiveMQ topic `factory/auth/request`) or the web dashboard login screen (received via HTTP REST `POST /api/v1/auth/login`). Verification executes inside an atomic PostgreSQL transaction using row-level locking (`SELECT ... FOR UPDATE`), checking `locked_until`, performing plaintext equality checks against `users.pin`, updating `failed_attempts` and 60-second `locked_until` timestamps after 2 consecutive failures, resetting failed counters on valid PIN entry, recording every attempt to the immutable `auth_audit_logs` table, and publishing formatted response JSON to `factory/auth/response` matching Board A's parser.

**Blocked by:** 01: Project Scaffolding, Database Migrations, and Health Probe

**Status:** ready-for-human

- [x] Unified `AuthService` handles authentication across both `KEYPAD` and `DASHBOARD` sources within a single PostgreSQL transaction with row-level locking (`SELECT ... FOR UPDATE`).
- [x] Plaintext PIN equality comparison against `users.pin`.
- [x] Active lockout detection: rejects attempts if `locked_until > NOW()` returning `USER_LOCKED` with remaining seconds.
- [x] Incorrect PIN handling: increments `failed_attempts`; if reaching 2, sets `locked_until = NOW() + 60s` and returns `INVALID_PIN` or `USER_LOCKED`.
- [x] Correct PIN handling: resets `failed_attempts = 0`, clears `locked_until`, and returns `AUTH_OK` with user's display name.
- [x] Non-existent User ID handling: returns `USER_NOT_FOUND` without mutating existing user lockout timers.
- [x] Append-only audit record inserted into `auth_audit_logs` on every authentication attempt (`source`, `user_id`, `status`, `timestamp`).
- [x] MQTT subscriber for `factory/auth/request` unmarshals requests and publishes response JSON to `factory/auth/response` matching Board A's `auth_protocol.cpp` expectations.
- [x] HTTP endpoint `POST /api/v1/auth/login` returns semantic status codes (200 OK, 401 Unauthorized, 403 Forbidden, 404 Not Found) with matching JSON response bodies.
- [x] Table-driven unit tests verify lockout transitions, 60s freeze expiry, attempt counts, and audit logging completely offline using mock interfaces.

## Comments

- Implemented unified `AuthService` in `internal/service/auth.go` with domain models, clock abstraction, row-level transaction boundaries, and immutable audit logging.
- Implemented `PostgresAuthRepository` in `internal/db/auth_repo.go` executing `SELECT ... FOR UPDATE` row locks, security state updates, and audit log inserts into `auth_audit_logs`.
- Implemented HTTP REST endpoint `POST /api/v1/auth/login` in `internal/api/auth_handler.go` with semantic HTTP response codes (200 OK, 401 Unauthorized, 403 Forbidden, 404 Not Found, 400 Bad Request, 500 Internal Error) and JSON response bodies.
- Implemented MQTT client and decoupled `AuthWorker` in `internal/mqtt` subscribing to `factory/auth/request` and publishing to `factory/auth/response` matching Gateway Board A's protocol framing.
- Wired `AuthService` and `AuthWorker` into `cmd/server/main.go` with graceful shutdown handling.
- Table-driven unit tests cover all lockout states, attempt count increments, 60s freeze expiry, and audit log entries across domain, API, and MQTT layers.

Status: ready-for-agent

# Golang Backend Service Specification

## Problem Statement

The distributed sorting, counting, and access control system requires a centralized cloud backend to orchestrate authentication, enforce per-user security lockouts, ingest computer vision shape detection webhooks, coordinate bi-directional MQTT communications with the Gateway microcontroller (`Board A`), and synchronize persistent telemetry state with a managed Supabase PostgreSQL database. Currently, the `backend-service/` repository is empty. Without this backend service, physical keypad entries on the Gateway cannot be authenticated, incoming shape detection webhooks cannot be filtered or dispatched to the field actuators, and web dashboard users cannot query real-time system state or control the sorting gate.

## Solution

Build an idiomatic, production-grade Go backend web service hosted on Render that bridges the external Vision API, HiveMQ Cloud MQTT broker, and Supabase PostgreSQL database:
1. **HTTP API (`chi/v5`)**: Exposes REST endpoints for vision detection webhook ingestion (`POST /api/v1/detections`), dashboard authentication (`POST /api/v1/auth/login`), remote actuator servo control (`POST /api/v1/actuator/servo`), consolidated system state queries (`GET /api/v1/state`), and health checks (`GET /healthz`), configured with CORS and structured logging.
2. **Unified Authentication Engine**: Enforces identical Two-Step Authentication verification and Per-User Lockout rules across both input surfaces (Keypad over MQTT and Dashboard over HTTP) using PostgreSQL row-level locks (`SELECT ... FOR UPDATE`) in a single transaction (per ADR-0005).
3. **MQTT Cloud Bridge (`paho.mqtt.golang`)**: Maintains a resilient TLS connection to HiveMQ Cloud, subscribing to `factory/auth/request`, `factory/telemetry`, and `factory/rollover`, while publishing to `factory/auth/response`, `factory/detections`, and `factory/actuator/servo` via decoupled worker goroutines.
4. **Detection Ingestion & Debounce Filter**: Ingests vision webhooks, validates Bearer tokens, maps shape strings to numeric IDs, enforces a thread-safe 2-second sliding window debounce cache, and publishes valid events with unique detection IDs.
5. **Authoritative Telemetry & Persistence Sync**: Synchronizes the `system_state` singleton table strictly from Board A's verified hardware telemetry (per ADR-0006), and atomically accumulates lifetime shape counts upon receiving Batch Rollover messages.

## User Stories

1. As a floor operator entering my credentials on the Gateway keypad, I want my submitted User ID and plaintext PIN to be validated by the backend via MQTT (`factory/auth/request`), so that I can gain authorized access at the physical terminal.
2. As a dashboard user entering my credentials on the web application, I want my submitted User ID and plaintext PIN to be validated via REST (`POST /api/v1/auth/login`), so that I can log in using the exact same credentials.
3. As a security supervisor, I want authentication verification to execute per-user lockout evaluation inside an atomic database transaction (`SELECT ... FOR UPDATE`), so that rapid concurrent attempts cannot produce race conditions.
4. As an operator entering an incorrect PIN, I want the backend to increment my user record's `failed_attempts` counter and return an `INVALID_PIN` status with remaining attempts (1 of 2), so that I know I have one attempt left.
5. As an operator failing my PIN twice consecutively, I want the backend to set my record's `locked_until` timestamp to 60 seconds into the future and return a `USER_LOCKED` status, so that brute-force credential guessing is blocked.
6. As a locked-out operator attempting to authenticate before the 60 seconds elapse, I want the backend to reject my attempt immediately with `USER_LOCKED` and the remaining lockout seconds, without altering other users' access.
7. As an operator with 1 failed attempt who submits the correct PIN on the second try, I want the backend to reset my `failed_attempts` counter to 0, clear any lockout timestamp, and return an `AUTH_OK` status with my username, so that my account remains in good standing.
8. As an operator submitting a non-existent User ID, I want the backend to return `USER_NOT_FOUND` without affecting any existing user accounts or lockout timers.
9. As a security auditor, I want every authentication attempt across both the Keypad and Dashboard to be recorded in the append-only `auth_audit_logs` table (`id`, `source`, `user_id`, `status`, `timestamp`), so that access history is fully traceable.
10. As an external computer vision service, I want to submit detected objects via `POST /api/v1/detections` with an `Authorization: Bearer <TOKEN>` header, so that unauthorized requests are rejected with HTTP 401.
11. As a vision ingestion system, I want the backend to accept shape identifiers as names (`circle`, `triangle`, `square`) or numeric IDs (1, 2, 3), so that client integration is flexible.
12. As a field sorting system, I want incoming shape detections to be evaluated against an in-memory 2-second debounce window per shape, so that duplicate camera frames within 2 seconds are dropped.
13. As an external vision camera webhook client, I want debounced duplicate submissions to return HTTP 200 OK with `{"status": "debounced"}`, so that the camera client does not trigger aggressive error retries.
14. As the Gateway microcontroller, I want valid shape detections to be published to HiveMQ topic `factory/detections` formatted as `{"shape_id": <int>, "shape_name": "<str>", "detection_id": <int>}`, so that Board A can relay them over ESP-NOW to Board B.
15. As the field Actuator completing a batch of 5 items, I want the Batch Rollover notification sent on `factory/rollover` to atomically increment `total_lifetime` by 5 and reset `live_buffer` to 0 in Supabase, so that aggregate counts are accurately preserved.
16. As a monitoring dashboard, I want Board A's periodic and event-driven `factory/telemetry` heartbeat to update the `system_state` singleton table (`is_paused`, `motor_state`, `servo_state`, `last_telemetry_at`) and live shape buffers, so that the database reflects verified physical reality.
17. As an authenticated dashboard operator, I want to send a gate command (`{"state": "OPEN"}` or `{"state": "CLOSED"}`) via `POST /api/v1/actuator/servo`, so that the backend publishes the instruction to HiveMQ topic `factory/actuator/servo`.
18. As a safety supervisor, I want `POST /api/v1/actuator/servo` to publish to MQTT without optimistically mutating `system_state.servo_state` in the database, ensuring the database reflects true actuator state when the machine is paused.
19. As a front-end web dashboard on startup, I want `GET /api/v1/state` to return a consolidated system snapshot (`system_state`, `shape_counts`, recent audit logs, and broker connection status), so that all initial telemetry is loaded in a single request.
20. As a cloud deployment platform (Render), I want `GET /healthz` to return service liveness and database/MQTT connectivity status, so that container health can be monitored.
21. As a developer deploying the service, I want embedded idempotent database migrations to automatically create tables and seed default users (`Andres` with ID 1 and PIN `1234`, `Aldo` with ID 2 and PIN `5678`), so that the system is operational immediately upon startup.

## Implementation Decisions

### Architectural Framework & HTTP Routing
- Built using Go standard library idioms and `go-chi/chi/v5` for lightweight routing, sub-routing, and middleware chaining.
- Standard middleware includes structured request logging, panic recovery, and CORS (`cors.Handler`) allowing browser requests from Vite/React frontends.
- Bearer token authentication middleware for protected endpoints (`/api/v1/detections`).

### Database Engine & Schema Migrations
- Uses `jackc/pgx/v5` with `pgxpool.Pool` for native PostgreSQL binary protocol support, connection pooling, and transactional isolation.
- Schema migrations and seed data are stored in pure SQL files and embedded into the binary using Go `embed.FS`, executing idempotently on application startup inside a transaction:
  - `users`: ID primary key, `username`, `pin` (plaintext), `failed_attempts`, `locked_until`, `created_at`. Seeded with Andres (ID 1, PIN 1234) and Aldo (ID 2, PIN 5678).
  - `system_state`: Enforced singleton (`CHECK (id = 1)`), storing `is_paused`, `motor_state`, `servo_state`, `last_telemetry_at`.
  - `shape_counts`: Pre-seeded with 3 records for Circle (1, red), Triangle (2, green), Square (3, blue), tracking `live_buffer` and `total_lifetime`.
  - `auth_audit_logs`: Append-only log storing UUID primary key, `source` (`KEYPAD` or `DASHBOARD`), `user_id`, `status` (`SUCCESS`, `INVALID_PIN`, `USER_LOCKED`, `USER_NOT_FOUND`), and `timestamp`.

### Unified Authentication Domain Engine
- Per ADR-0005, a unified `AuthService` handles authentication logic for both Keypad (MQTT) and Dashboard (HTTP REST).
- Queries `SELECT * FROM users WHERE id = $1 FOR UPDATE` to serialize concurrent requests for the same User ID.
- Checks `locked_until`: If `locked_until > NOW()`, rejects immediately with `USER_LOCKED` and remaining seconds.
- Validates PIN:
  - Match: resets `failed_attempts = 0`, sets `locked_until = NULL`, commits transaction, logs `SUCCESS`.
  - Mismatch: increments `failed_attempts`. If `failed_attempts >= 2`, sets `locked_until = NOW() + INTERVAL '60 SECONDS'`. Commits transaction, logs `INVALID_PIN` or `USER_LOCKED`.
- Non-existent user: logs `USER_NOT_FOUND` without mutating user table.

### Vision Ingestion & Debounce Filter
- `POST /api/v1/detections` validates the Bearer token against `VISION_BEARER_TOKEN`.
- Translates shape names (`circle`, `triangle`, `square`) to shape IDs (1, 2, 3).
- Uses a thread-safe in-memory cache (`sync.Mutex` with `map[uint8]time.Time`) tracking the last detection timestamp per shape ID:
  - If `now - lastTime < 2 * time.Second`: returns HTTP 200 with `{"status": "debounced", "shape_id": X, "message": "Duplicate detection dropped within 2s debounce window"}`.
  - If valid: generates an atomic monotonic/timestamp-based `detection_id`, updates the timestamp map, and publishes `{"shape_id": X, "shape_name": Y, "detection_id": Z}` to HiveMQ topic `factory/detections`.

### MQTT Client & Decoupled Workers
- Eclipse Paho Go (`github.com/eclipse/paho.mqtt.golang`) connects over TLS to HiveMQ Cloud with auto-reconnect, keepalive, and clean session management.
- Incoming MQTT callbacks decode messages and push tasks into buffered Go channels consumed by background worker goroutines, isolating the MQTT network keepalive loop from database query latencies.
- Subscribes to:
  - `factory/auth/request` $\rightarrow$ calls `AuthService.Authenticate(...)` with `source="KEYPAD"` $\rightarrow$ publishes response to `factory/auth/response`.
  - `factory/telemetry` $\rightarrow$ calls `StateService.UpdateTelemetry(...)` $\rightarrow$ updates `system_state` and live shape counts.
  - `factory/rollover` $\rightarrow$ calls `StateService.ProcessRollover(...)` $\rightarrow$ atomically executes `total_lifetime = total_lifetime + 5` and resets `live_buffer = 0`.
- Publishes to:
  - `factory/auth/response`: `{"status": "<enum>", "username": "<str>", "remaining_attempts": <int>, "lockout_seconds": <int>}`.
  - `factory/detections`: `{"shape_id": <int>, "shape_name": "<str>", "detection_id": <int>}`.
  - `factory/actuator/servo`: `"OPEN"` or `"CLOSED"`.

### Authoritative Telemetry Synchronization
- Per ADR-0006, `POST /api/v1/actuator/servo` commands the physical servo gate by publishing to HiveMQ, but does not optimistically mutate `system_state.servo_state`.
- Hardware telemetry published by Board A on `factory/telemetry` remains the sole authority for `system_state`, respecting the physical Machine Pause lockout.

### Configuration Management
- Configuration loaded from environment variables and optional `.env` file:
  - `PORT`: HTTP port (default `8080`).
  - `DATABASE_URL`: PostgreSQL connection URI (`postgres://...`).
  - `MQTT_BROKER_HOST`: HiveMQ cluster host.
  - `MQTT_BROKER_PORT`: MQTT TLS port (default `8883`).
  - `MQTT_USERNAME` / `MQTT_PASSWORD`: HiveMQ credentials.
  - `MQTT_CLIENT_ID`: Unique client ID (default `backend-service-go`).
  - `VISION_BEARER_TOKEN`: Authorization token for the vision webhook.
  - `CORS_ALLOWED_ORIGINS`: Allowed origins (default `*`).

## Testing Decisions

### What Makes a Good Test
- Tests must verify external behavioral contracts, business rules, and HTTP/MQTT protocol framing rather than internal implementation minutiae.
- Business logic must be executable completely offline without external network or database dependencies by using clean repository and publisher interfaces.

### Seams and Test Boundaries
1. **HTTP API Handler Seam (`http/router_test.go`)**:
   - Highest seam for the web API layer using `net/http/httptest.ResponseRecorder` and `chi.Router`.
   - Verifies Bearer token auth enforcement, route matching, request payload deserialization, debounce responses (200 debounced vs 200 dispatched), and response status codes (200, 401, 403, 404).
2. **Domain Service & Concurrency Seam (`service/auth_test.go`, `service/detection_test.go`, `service/state_test.go`)**:
   - Tests `AuthService` lockout state transitions:
     - Correct PIN on first try $\rightarrow$ `AUTH_OK`.
     - 1 incorrect PIN $\rightarrow$ `INVALID_PIN` with 1 remaining attempt.
     - 2 incorrect PINs $\rightarrow$ `USER_LOCKED` with 60-second timer.
     - Subsequent attempts within 60 seconds $\rightarrow$ `USER_LOCKED` with decremented timer.
     - Attempt after 60 seconds $\rightarrow$ allowed to retry.
     - Audit logs properly recorded with source (`KEYPAD` vs `DASHBOARD`).
   - Tests `DetectionService` debounce timing:
     - Rapid duplicate detections within 2 seconds are filtered.
     - Detections for different shapes are not blocked by each other.
     - Detections arriving after 2 seconds pass successfully.
   - Tests `StateService` atomic rollover increment and telemetry updates.
3. **MQTT Protocol Framing & Dispatcher Seam (`mqtt/worker_test.go`)**:
   - Verifies JSON unmarshaling and validation for incoming `factory/auth/request`, `factory/telemetry`, and `factory/rollover` payloads.
   - Verifies outgoing payload formatting matches Gateway Board A's parser in `auth_protocol.cpp`.

## Out of Scope

- User registration endpoints or password hashing (system specification explicitly mandates plaintext PIN storage and direct equality checks for physical keypad simulation).
- Direct ESP-NOW or radio communication (handled exclusively by Gateway Board A).
- Direct computer vision inference or camera video stream processing (handled by external Vision API service).
- Direct WebSocket push server (handled by frontend subscribing directly to HiveMQ WebSockets or querying `GET /api/v1/state`).

## Further Notes

- The backend service lives in `/mnt/Data/Dev/UNI/robotica/parcial1/backend-service` as an independent Go module with its own git repository.
- Follows the single-context domain model in [CONTEXT.md](file:///mnt/Data/Dev/UNI/robotica/parcial1/CONTEXT.md) and architectural decisions in [ADR-0005](file:///mnt/Data/Dev/UNI/robotica/parcial1/docs/adr/0005-unified-auth-engine-row-level-locking.md) and [ADR-0006](file:///mnt/Data/Dev/UNI/robotica/parcial1/docs/adr/0006-authoritative-telemetry-system-state.md).

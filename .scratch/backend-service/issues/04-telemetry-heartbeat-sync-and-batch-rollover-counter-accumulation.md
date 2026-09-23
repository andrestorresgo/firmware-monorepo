# 04: Telemetry Heartbeat Sync and Batch Rollover Counter Accumulation

**What to build:** Background MQTT subscribers and database synchronization routines for hardware telemetry and rollover notifications from Board A. The MQTT worker listens on `factory/telemetry` to decode JSON telemetry frames (`is_paused`, `motor_state`, `servo_state`, `red_count`, `green_count`, `blue_count`), updating the singleton `system_state` record and current `live_buffer` values in `shape_counts`. When Board A publishes a batch completion event on `factory/rollover`, the worker executes an atomic SQL increment in Supabase (`total_lifetime = total_lifetime + 5`, `live_buffer = 0`), ensuring aggregate production numbers are safely accumulated without race conditions.

**Blocked by:** 01: Project Scaffolding, Database Migrations, and Health Probe

**Status:** ready-for-human

- [x] MQTT subscriber for `factory/telemetry` decodes live peripheral status and binary counter values from Board A.
- [x] Updates singleton `system_state` row (`is_paused`, `motor_state`, `servo_state`, `last_telemetry_at = NOW()`).
- [x] Synchronizes `shape_counts.live_buffer` with current counts (`red_count` for Circle, `green_count` for Triangle, `blue_count` for Square).
- [x] MQTT subscriber for `factory/rollover` decodes batch rollover events (`shape_id`, `shape_name`, `timestamp`).
- [x] Atomically increments `total_lifetime` by 5 and resets `live_buffer = 0` in `shape_counts` for the corresponding `shape_id`.
- [x] Database query operations are decoupled from MQTT network receive callbacks via buffered Go channels.
- [x] Unit tests verify telemetry unmarshaling, database repository update calls, atomic increment SQL queries, and error handling.

## Comments

- Implemented `StateService` in `internal/service/state.go` managing authoritative telemetry updates and atomic batch rollover counter accumulation, including live counter clamping (`[0, 5]`) to satisfy PostgreSQL check constraints and case-insensitive shape name-to-ID fallback resolution.
- Implemented `PostgresStateRepository` in `internal/db/state_repo.go` with transactional updates for `system_state` and `shape_counts.live_buffer`, and an atomic SQL statement for rollover accumulation (`total_lifetime = total_lifetime + 5`, `live_buffer = 0`).
- Implemented `TelemetryWorker` and `RolloverWorker` in `internal/mqtt/telemetry_worker.go` decoupling MQTT message arrival callbacks from database execution via buffered Go channels.
- Added topic constants (`factory/telemetry`, `factory/rollover`, `factory/actuator/servo`) and payload structs (`TelemetryPayload`, `BatchRolloverPayload`) in `internal/mqtt/types.go` matching Gateway Board A's protocol framing.
- Added subscription methods `SubscribeTelemetry` and `SubscribeRollover` to `Client` in `internal/mqtt/client.go`.
- Wired `StateRepository`, `StateService`, `TelemetryWorker`, and `RolloverWorker` in `cmd/server/main.go` with graceful shutdown signal propagation.
- Comprehensive unit tests added in `internal/service/state_test.go`, `internal/db/state_repo_test.go`, and `internal/mqtt/telemetry_worker_test.go`, passing all tests with `-race`.


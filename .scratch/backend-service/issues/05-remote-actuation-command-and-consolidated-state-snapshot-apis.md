# 05: Remote Actuation Command and Consolidated State Snapshot APIs

**What to build:** Web dashboard REST integration endpoints for remote physical control and initial state synchronization. Exposes `POST /api/v1/actuator/servo` allowing authenticated dashboard operators to command the sorting gate between `OPEN` and `CLOSED`. Validates payload strings or boolean flags and publishes the command to HiveMQ topic `factory/actuator/servo` without optimistically mutating `system_state` (per ADR-0006), ensuring physical reality reported by telemetry remains the sole authority. Exposes `GET /api/v1/state` delivering a consolidated system snapshot (`system_state`, all three `shape_counts`, recent records from `auth_audit_logs`, and MQTT broker connection status) to populate dashboard telemetry in a single request on page load. Implements clean server shutdown with context propagation.

**Blocked by:** 02: Unified Authentication Engine and Lockout across MQTT and REST, 04: Telemetry Heartbeat Sync and Batch Rollover Counter Accumulation

**Status:** ready-for-human

- [x] HTTP endpoint `POST /api/v1/actuator/servo` accepts `{"state": "OPEN"|"CLOSED"}` or `{"open": bool}`.
- [x] Publishes command to HiveMQ topic `factory/actuator/servo` matching Board A's parser in `auth_protocol.cpp`.
- [x] Respects ADR-0006 by leaving database `system_state.servo_state` unchanged until verified telemetry arrives from Board A.
- [x] Returns HTTP 200 OK with `{"status": "dispatched", "state": "OPEN"|"CLOSED"}` on successful publication.
- [x] HTTP endpoint `GET /api/v1/state` returns a consolidated snapshot containing:
  - `system_state` (is_paused, motor_state, servo_state, last_telemetry_at)
  - `shape_counts` (all 3 records with live_buffer and total_lifetime)
  - `recent_audits` (latest 10 entries from `auth_audit_logs`)
  - `mqtt_connected` (boolean broker connection status)
- [x] Graceful application shutdown handling `SIGINT`/`SIGTERM`, flushing pending MQTT messages and closing database pools cleanly.
- [x] Unit and router tests verify request handling, JSON responses, error cases, and MQTT publication dispatch.

## Comments

- Implemented `ActuatorService` in `internal/service/actuator.go` accepting either `{"state": "OPEN"|"CLOSED"}` (case-insensitive) or `{"open": bool}`, validating commands and publishing to HiveMQ topic `factory/actuator/servo` without mutating `system_state` per ADR-0006.
- Implemented `GetRecentAudits` in `PostgresStateRepository` (`internal/db/state_repo.go`) querying latest 10 rows from `auth_audit_logs` with safe nullable `user_id` scanning.
- Implemented `GetSnapshot` in `StateService` (`internal/service/state.go`) combining authoritative `system_state`, `shape_counts`, recent audits, and `mqtt_connected` boolean status with non-nil slice defaulting.
- Implemented REST handlers `ActuatorServoHandler` (`POST /api/v1/actuator/servo`) and `StateHandler` (`GET /api/v1/state`) in `internal/api/`.
- Registered routes in `internal/api/router.go` and wired `ActuatorService`, `StateService`, and `mqttClient` into `cmd/server/main.go`.
- Refined graceful shutdown sequence in `cmd/server/main.go` to drain incoming HTTP requests before stopping background MQTT workers and closing database connection pools.
- Added comprehensive unit and router test suites across `internal/service/actuator_test.go`, `internal/api/actuator_handler_test.go`, `internal/api/state_handler_test.go`, and updated `internal/service/state_test.go`, `internal/db/state_repo_test.go`, and `internal/api/router_test.go`, passing all tests with `-race`.


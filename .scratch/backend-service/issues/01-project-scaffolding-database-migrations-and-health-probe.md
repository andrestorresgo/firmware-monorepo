# 01: Project Scaffolding, Database Migrations, and Health Probe

**What to build:** The foundational module structure, configuration loading, database migration engine, and health probe endpoint for the Go backend service. The service loads 12-factor configuration from environment variables or `.env` files with safe defaults. Upon connecting to PostgreSQL (Supabase) via a high-performance connection pool (`pgxpool`), the service runs embedded SQL migrations to idempotently create the schema (`users`, `system_state`, `shape_counts`, `auth_audit_logs`) and seed default operators (`Andres` with ID 1 and PIN `1234`, `Aldo` with ID 2 and PIN `5678`), the singleton `system_state` record, and three shape records. The service runs a `chi/v5` router with structured logging, recovery, and CORS middleware, exposing a `GET /healthz` endpoint returning service liveness and database connection status.

**Blocked by:** None (can start immediately)

**Status:** ready-for-human

- [x] Go module initialized in `backend-service/` with required dependencies (`go-chi/chi/v5`, `jackc/pgx/v5`, `paho.mqtt.golang`, `joho/godotenv`).
- [x] 12-factor configuration module loading `PORT`, `DATABASE_URL`, `MQTT_*`, `VISION_BEARER_TOKEN`, and `CORS_ALLOWED_ORIGINS` with fallback defaults.
- [x] Embedded idempotent SQL migration script creating tables for `users`, `system_state`, `shape_counts`, and `auth_audit_logs`.
- [x] Idempotent seed data inserting users `Andres` (ID 1, PIN `1234`) and `Aldo` (ID 2, PIN `5678`), singleton `system_state` (ID 1), and initial `shape_counts` for Circle, Triangle, and Square.
- [x] HTTP server running `chi/v5` with CORS, panic recovery, and request logging.
- [x] `GET /healthz` endpoint returns HTTP 200 with service health and database connectivity details.
- [x] Unit tests verify configuration parsing, migration execution, and HTTP health endpoint responses.

## Comments

- Initialized module `github.com/andrestorresgo/backend-service` in `backend-service/`.
- Implemented 12-factor configuration module in `internal/config` supporting environment variables and `.env` with fallback defaults.
- Implemented embedded SQL migrations in `internal/db` with `001_init.sql` declaring `users`, `system_state`, `shape_counts`, and `auth_audit_logs` tables along with idempotent seed data for operators Andres and Aldo, singleton system state, and default shapes.
- Created `chi/v5` router with request ID, real IP, logger, recoverer, CORS middleware, and `GET /healthz` probe.
- All unit tests passing with `-race` detection and zero `go vet` issues.


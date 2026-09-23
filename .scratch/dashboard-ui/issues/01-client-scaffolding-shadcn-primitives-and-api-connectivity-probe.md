# 01: Client Scaffolding, Shadcn Primitives, and API Connectivity Probe

**What to build:** The foundational dependencies, shadcn UI components, 12-factor configuration module, and backend connectivity health check for the dashboard web application. Configures Vite environment variables (`VITE_API_URL`, `VITE_MQTT_BROKER_HOST`, `VITE_MQTT_WS_PORT`, `VITE_MQTT_WS_PATH`, `VITE_MQTT_USERNAME`, `VITE_MQTT_PASSWORD`) with safe defaults. Installs required Base UI shadcn components (`card`, `badge`, `alert`, `separator`, `input`, `skeleton`) using `bunx --bun shadcn@latest add` and the `mqtt` library via `bun add`. Creates the HTTP API client module with typed request/response handlers for the Go backend (`/healthz`, `/api/v1/state`, `/api/v1/auth/login`, `/api/v1/actuator/servo`). Renders an initial connectivity status bar demonstrating live reachability of the Go backend and verifying TypeScript type checking and build integrity.

**Blocked by:** None (can start immediately)

**Status:** resolved

- [x] Add `mqtt` dependency using `bun add mqtt` and `@types/mqtt` if needed.
- [x] Add required shadcn base components (`card`, `badge`, `alert`, `separator`, `input`, `skeleton`) using `bunx --bun shadcn@latest add`.
- [x] Create 12-factor configuration service reading `import.meta.env` with fallback defaults (`http://localhost:8080`, `broker.hivemq.com:8884/mqtt`).
- [x] Implement typed HTTP client for Go backend endpoints (`/healthz`, `/api/v1/state`, `/api/v1/auth/login`, `/api/v1/actuator/servo`).
- [x] Render initial connectivity probe header verifying live backend health status with responsive indicators.
- [x] `bun run typecheck` and `bun run build` pass with zero errors.

## Comments

- Installed `mqtt` (v5.16.0) and Base UI shadcn primitives (`card`, `badge`, `alert`, `separator`, `input`, `skeleton`).
- Implemented `src/config/env.ts` reading 12-factor environment variables with test coverage in `src/config/env.test.ts`. Created `.env.example`.
- Implemented `src/types/api.ts` and `src/lib/api-client.ts` with test coverage in `src/lib/api-client.test.ts` covering `/healthz`, `/api/v1/state`, `/api/v1/auth/login`, and `/api/v1/actuator/servo`.
- Implemented `ConnectivityHeader` component adhering to semantic shadcn tokens with live backend reachability probe, database and MQTT sub-badges, theme toggle, and auto-refresh.
- Validated with `bun test` (10 passing tests), `bun run typecheck`, `bun run lint`, and `bun run build` with zero errors.

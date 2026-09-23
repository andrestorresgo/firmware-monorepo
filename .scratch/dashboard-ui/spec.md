Status: ready-for-agent

# Web Dashboard Specification

## Problem Statement

The distributed sorting, counting, and access control system coordinates physical actuation, computer vision shape detections, and access control across an edge Gateway (`Board A`), an Actuator (`Board B`), a Go backend service, and HiveMQ Cloud. While the firmware and Go backend services are implemented, operators currently lack an operational web interface to monitor live telemetry, supervise physical actuation, and audit terminal access. Without this web dashboard, floor supervisors cannot remotely observe the 3-bit binary LED counting cycles in real-time, safely command the sorting servo gate, respond to machine-level safety halts (Machine Pause), or monitor credential lockout events.

## Solution

Build a high-performance, responsive React single-page application in `dashboard-ui/` using Vite, TypeScript, Tailwind CSS v4, Base UI (`@base-ui/react`), and shadcn (`base-maia` style with `emerald` theme), managed via `bun`:
1. **Strict Semantic Styling**: Exclusively utilizes shadcn default theme tokens (`primary`, `destructive`, `secondary`, `accent`, `muted`, `card`, `border`). Zero ad-hoc or un-tokenized raw CSS colors.
2. **Hybrid Telemetry Transport (ADR-0007)**: Hydrates initial machine state, shape counters, and audit history via Go backend REST snapshot (`GET /api/v1/state`), then maintains a direct secure WebSocket connection (`wss://`) to HiveMQ Cloud on `factory/telemetry` and `factory/rollover` for sub-second visual updates, falling back to REST polling if the WebSocket degrades.
3. **Two-Step Operator Authentication**: Provides a dedicated login gate with quick-select operator badges (`Andres - ID 1`, `Aldo - ID 2`) and manual numeric User ID entry. Enforces per-user lockout rules with an active real-time 1-second countdown timer disabling inputs on `403 Forbidden` (`USER_LOCKED`). Persists authenticated sessions in `localStorage`.
4. **Machine Pause Safety Envelope (ADR-0004)**: When telemetry indicates `is_paused: true`, renders a prominent emergency alert banner, disables remote servo gate actuation with an explanatory safety tooltip, and updates DC motor drive status to `HALTED`.
5. **Authoritative Actuation Control (ADR-0006, ADR-0008)**: Controls the sorting servo gate via `POST /api/v1/actuator/servo`. Enters a pending acknowledgment state without optimistic toggle, committing the switch position only upon receiving verified hardware telemetry from Board A, with a 3-second timeout fallback.
6. **Shape Telemetry Cards**: Renders 3 cards for Circle (Red / `destructive`), Triangle (Green / `primary`), and Square (Blue / `secondary`), featuring 3-bit binary indicators ($2^2, 2^1, 2^0$), live buffer values (0 to 5), and cumulative database lifetime totals.
7. **Access Audit Trail**: Renders recent authentication logs across both physical `KEYPAD` and web `DASHBOARD` surfaces with semantic outcome badges.

## User Stories

1. As an operator opening the dashboard, I want to see a clear login view requiring my User ID and PIN, so that unauthorized personnel cannot command machine actuators.
2. As an operator, I want quick-select presets for configured operators (Andres ID 1, Aldo ID 2) alongside a numeric ID input, so that I can sign in quickly on touchscreens or keyboards.
3. As an operator entering an incorrect PIN, I want to see an invalid PIN warning indicating my remaining attempts (1 remaining), so that I know my credential status.
4. As an operator who has exceeded 2 failed attempts, I want to see an active real-time countdown timer (e.g. `Locked Out: 59s...`) that disables input until expiration, matching the behavior of Board A's OLED screen.
5. As an authenticated operator, I want my session to persist in `localStorage` across page reloads, with my username clearly displayed in the header and an accessible "Sign Out" button.
6. As a floor supervisor, I want to see the system connection health at all times (Backend API and HiveMQ WebSocket broker), with automatic fallback to polling if WebSockets disconnect.
7. As a supervisor, I want to see a prominent emergency banner when physical Machine Pause is engaged by the push button on Board B, so that safety interruptions are immediately recognizable.
8. As an operator while Machine Pause is active, I want the Servo Gate switch to be disabled with a safety lockout explanation, preventing impossible physical commands.
9. As an operator, I want to toggle the physical sorting servo gate between Open and Closed states via `POST /api/v1/actuator/servo`, seeing a pending indicator until Board A's telemetry confirms the physical gate movement.
10. As an operator, I want to see the DC motor drive state displayed in real-time as `RUNNING` or `HALTED`.
11. As a line supervisor, I want to view 3 shape telemetry cards (Circle, Triangle, Square) displaying 3-bit binary indicators ($2^2, 2^1, 2^0$) that visually mirror the physical LED banks on Board B.
12. As a line supervisor, I want each shape card to show both the live unrolled buffer (0 to 5) and the cumulative database lifetime count, updated instantly upon Batch Rollover events.
13. As a security auditor, I want to inspect the Access Audit Trail table showing recent authentication attempts across KEYPAD and DASHBOARD with timestamps and outcome badges (`SUCCESS`, `INVALID_PIN`, `USER_LOCKED`, `USER_NOT_FOUND`).

## Implementation Decisions

- **Vite & Tailwind CSS v4**: Built inside `dashboard-ui/` using `@base-ui/react` and shadcn `base-maia` style with `emerald` theme.
- **Strict Default Styling Tokens**: Semantic classes only (`bg-card`, `text-card-foreground`, `bg-destructive`, `bg-primary`, `bg-secondary`, `bg-muted`).
- **Direct MQTT over WebSockets**: Uses the `mqtt` package to connect to HiveMQ Cloud WebSocket port (`wss://broker.hivemq.com:8884/mqtt`).
- **Authoritative Acknowledgment**: Switch controls do not use optimistic UI mutations; they reflect physical verified hardware telemetry per ADR-0006 and ADR-0008.

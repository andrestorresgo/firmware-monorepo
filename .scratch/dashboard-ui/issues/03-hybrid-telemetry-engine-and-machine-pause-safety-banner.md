# 03: Hybrid Telemetry Engine and Machine Pause Safety Banner

**What to build:** The real-time telemetry state management and safety alert engine. Upon loading the authenticated dashboard, hydrates full system state via `GET /api/v1/state` (`system_state`, `shape_counts`, `recent_audits`, `mqtt_connected`). Establishes a direct WSS connection to HiveMQ Cloud subscribing to `factory/telemetry` and `factory/rollover` using the `mqtt` browser client. If the WebSocket connection fails or drops, smoothly switches to transparent periodic polling of `GET /api/v1/state` every 2.5 seconds, auto-recovering when the WebSocket reconnects. Renders a system status header with broker connectivity indicators and a high-visibility Machine Pause emergency banner (`variant="destructive"`) when `system_state.is_paused` is true, displaying the DC gear motor status badge as `RUNNING` or `HALTED / DE-ENERGIZED`.

**Blocked by:** 02: Two-Step Operator Authentication Gate and Per-User Lockout

**Status:** closed

- [x] Create central telemetry store/hook managing authoritative `system_state`, `shape_counts`, and `recent_audits`.
- [x] Implement initial state hydration calling `GET /api/v1/state` on dashboard mount.
- [x] Implement direct HiveMQ Cloud WebSocket connection subscribing to `factory/telemetry` and `factory/rollover`.
- [x] Implement seamless polling fallback (every 2.5s) when WebSocket connection is offline or degraded.
- [x] Render system status bar with live MQTT connection indicator and DC motor drive status (`RUNNING` / `HALTED`).
- [x] Render emergency alert banner (`variant="destructive"`) at the top of the dashboard whenever `is_paused: true` is reported.
- [x] Verification tests for state hydration, telemetry decoding, and fallback polling behavior.

## Comments
- Implemented `TelemetryEngine` and pure state reducer `applyTelemetryMessage`/`applyRolloverMessage`.
- Established direct HiveMQ Cloud WebSocket connection with automatic 2.5s fallback REST polling during disconnection.
- Created `MachinePauseBanner` (`variant="destructive"`) and `SystemStatusBar` displaying live DC motor drive (`RUNNING` / `HALTED / DE-ENERGIZED`) and transport channel indicators.
- Verified across 53 unit tests passing in `dashboard-ui`.

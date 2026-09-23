# 05: Authoritative Servo Gate Actuation and Access Audit Trail

**What to build:** The remote physical actuation control for the sorting servo gate and the security access audit trail table. Provides an interactive toggle switch to command the sorting gate between Open and Closed states. Adheres strictly to ADR-0006 and ADR-0008: clicking the switch dispatches `POST /api/v1/actuator/servo` and places the switch into a pending/disabled loading state without optimistic toggling. The switch only commits to the new position when an authoritative telemetry frame arrives from Board A confirming `servo_state`. If unacknowledged within 3 seconds, or if Machine Pause is active (ADR-0004), the control locks and reverts with an explanatory alert. Renders an Access Audit Trail table below the telemetry grid displaying recent authentication attempts (`source`, `user_id`, `status` badge, `timestamp`), updated dynamically on snapshot refreshes and live logins.

**Blocked by:** 04: Shape Telemetry Cards with 3-Bit Binary Indicators and Rollover

**Status:** ready-for-agent

- [ ] Add shadcn `switch` component using `bunx --bun shadcn@latest add switch`.
- [ ] Implement authoritative servo control switch with pending acknowledgment state and 3-second timeout fallback.
- [ ] Enforce total actuation lockout on the switch when `is_paused: true` with a clear safety tooltip.
- [ ] Connect switch action to `POST /api/v1/actuator/servo` (`{ "state": "OPEN" | "CLOSED" }`).
- [ ] Build `AccessAuditTrail` component displaying recent logins with source badges (`KEYPAD` vs `DASHBOARD`), user ID/name, semantic status badges (`SUCCESS`, `INVALID_PIN`, `USER_LOCKED`, `USER_NOT_FOUND`), and formatted timestamps.
- [ ] Integrate all components into the cohesive single-screen industrial control room layout.
- [ ] End-to-end verification of remote servo dispatch, safety lockout, and audit log rendering.

## Comments

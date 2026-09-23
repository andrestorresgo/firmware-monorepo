# 05: Authoritative Servo Gate Actuation and Access Audit Trail

**What to build:** The remote physical actuation control for the sorting servo gate and the security access audit trail table. Provides an interactive toggle switch to command the sorting gate between Open and Closed states. Adheres strictly to ADR-0006 and ADR-0008: clicking the switch dispatches `POST /api/v1/actuator/servo` and places the switch into a pending/disabled loading state without optimistic toggling. The switch only commits to the new position when an authoritative telemetry frame arrives from Board A confirming `servo_state`. If unacknowledged within 3 seconds, or if Machine Pause is active (ADR-0004), the control locks and reverts with an explanatory alert. Renders an Access Audit Trail table below the telemetry grid displaying recent authentication attempts (`source`, `user_id`, `status` badge, `timestamp`), updated dynamically on snapshot refreshes and live logins.

**Blocked by:** 04: Shape Telemetry Cards with 3-Bit Binary Indicators and Rollover

**Status:** closed

- [x] Add shadcn `switch` component using `bunx --bun shadcn@latest add switch`.
- [x] Implement authoritative servo control switch with pending acknowledgment state and 3-second timeout fallback.
- [x] Enforce total actuation lockout on the switch when `is_paused: true` with a clear safety tooltip.
- [x] Connect switch action to `POST /api/v1/actuator/servo` (`{ "state": "OPEN" | "CLOSED" }`).
- [x] Build `AccessAuditTrail` component displaying recent logins with source badges (`KEYPAD` vs `DASHBOARD`), user ID/name, semantic status badges (`SUCCESS`, `INVALID_PIN`, `USER_LOCKED`, `USER_NOT_FOUND`), and formatted timestamps.
- [x] Integrate all components into the cohesive single-screen industrial control room layout.
- [x] End-to-end verification of remote servo dispatch, safety lockout, and audit log rendering.

## Comments
- Added official shadcn `switch`, `tooltip`, and `table` components via CLI.
- Implemented `ServoActuationController` in `src/lib/servo-actuation.ts` and `useServoActuation` hook conforming strictly to ADR-0004, ADR-0006, and ADR-0008.
- Built `ServoGateControl` component featuring non-optimistic toggle switch, awaiting Board A acknowledgment spinner, 3.0s timeout fallback with alert, and total actuation lockout when `is_paused: true` with safety tooltip.
- Built `AccessAuditTrail` component displaying authentication audit records in a styled table with `KEYPAD` vs `DASHBOARD` source badges, operator identity resolution (`Andres`, `Aldo`), semantic outcome badges (`SUCCESS`, `INVALID_PIN`, `USER_LOCKED`, `USER_NOT_FOUND`), and live refresh integration.
- Integrated `ServoGateControl` and `AccessAuditTrail` into `AuthenticatedWorkspace` in `App.tsx` completing the unified industrial control room layout.
- Added 18 new automated tests across `src/lib/servo-actuation.test.ts`, `src/components/actuator/servo-gate-control.test.tsx`, and `src/components/audit/access-audit-trail.test.tsx`. Full test suite of 94 tests passes cleanly.

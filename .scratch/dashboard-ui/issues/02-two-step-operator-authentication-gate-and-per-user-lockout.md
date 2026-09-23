# 02: Two-Step Operator Authentication Gate and Per-User Lockout

**What to build:** The complete operator login surface and session lifecycle management. Renders a clean Two-Step Authentication card featuring quick-select presets for configured operators (`Andres - ID 1`, `Aldo - ID 2`) along with an alternative numeric User ID input field and a masked/numeric PIN input field. Dispatches credentials to `POST /api/v1/auth/login`. When the backend returns HTTP 401 (`INVALID_PIN`), displays an invalid PIN alert with remaining attempts (1 attempt left). When the backend returns HTTP 403 (`USER_LOCKED`), freezes inputs for that specific operator and runs an active real-time 1-second countdown timer (e.g. `User Locked! Retry in 58s...`), automatically unfreezing when expired. Upon receiving HTTP 200 (`AUTH_OK`), saves the session in `localStorage`, displays the active operator in the dashboard header, and provides an accessible "Sign Out" button that clears the session and returns to the login gate.

**Blocked by:** 01: Client Scaffolding, Shadcn Primitives, and API Connectivity Probe

**Status:** resolved

- [x] Build Operator Login card with operator preset buttons (`Andres - ID 1`, `Aldo - ID 2`) and numeric ID input toggle.
- [x] Connect form submission to `POST /api/v1/auth/login` with typed error and status handling.
- [x] Implement active real-time countdown timer hook and alert for `USER_LOCKED` (HTTP 403) that freezes inputs and counts down to 0.
- [x] Display attempt warning banner on `INVALID_PIN` (HTTP 401) showing remaining attempts.
- [x] Persist authenticated operator state (`user_id`, `username`, timestamp) in `localStorage` upon `AUTH_OK`.
- [x] Render authenticated header showing active operator badge and "Sign Out" action button.
- [x] Unit/component verification testing for login, lockout timer countdown, and session persistence.

## Comments

- Implemented `OperatorLoginCard` with configured operator preset quick-select buttons (`Andres - ID 1`, `Aldo - ID 2`), manual numeric ID input toggle, and masked PIN input.
- Connected authentication submission to `apiClient.login` handling HTTP 200 (`AUTH_OK`), HTTP 401 (`INVALID_PIN`), HTTP 403 (`USER_LOCKED`), HTTP 404 (`USER_NOT_FOUND`), and network error states.
- Implemented `src/lib/session.ts` for safe `localStorage` session read/save/clear and per-user lockout timestamp persistence.
- Implemented pure lockout computation engine `src/lib/lockout.ts` and `useLockoutTimer` React hook with active 1-second real-time countdown timer, automatic unfreezing at 0s, and isolated per-operator lockout tracking.
- Updated `ConnectivityHeader` to render active operator badge with `UserCheck` icon and accessible "Sign Out" action button clearing the session.
- Updated `App.tsx` gating dashboard view based on operator session.
- Added comprehensive unit tests in `src/lib/session.test.ts`, `src/lib/lockout.test.ts`, and `src/components/auth/operator-login.test.ts`. All 28 tests passing with zero typecheck, lint, or build errors.

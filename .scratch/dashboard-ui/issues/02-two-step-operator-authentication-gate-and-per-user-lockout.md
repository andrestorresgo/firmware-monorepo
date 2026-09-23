# 02: Two-Step Operator Authentication Gate and Per-User Lockout

**What to build:** The complete operator login surface and session lifecycle management. Renders a clean Two-Step Authentication card featuring quick-select presets for configured operators (`Andres - ID 1`, `Aldo - ID 2`) along with an alternative numeric User ID input field and a masked/numeric PIN input field. Dispatches credentials to `POST /api/v1/auth/login`. When the backend returns HTTP 401 (`INVALID_PIN`), displays an invalid PIN alert with remaining attempts (1 attempt left). When the backend returns HTTP 403 (`USER_LOCKED`), freezes inputs for that specific operator and runs an active real-time 1-second countdown timer (e.g. `User Locked! Retry in 58s...`), automatically unfreezing when expired. Upon receiving HTTP 200 (`AUTH_OK`), saves the session in `localStorage`, displays the active operator in the dashboard header, and provides an accessible "Sign Out" button that clears the session and returns to the login gate.

**Blocked by:** 01: Client Scaffolding, Shadcn Primitives, and API Connectivity Probe

**Status:** ready-for-agent

- [ ] Build Operator Login card with operator preset buttons (`Andres - ID 1`, `Aldo - ID 2`) and numeric ID input toggle.
- [ ] Connect form submission to `POST /api/v1/auth/login` with typed error and status handling.
- [ ] Implement active real-time countdown timer hook and alert for `USER_LOCKED` (HTTP 403) that freezes inputs and counts down to 0.
- [ ] Display attempt warning banner on `INVALID_PIN` (HTTP 401) showing remaining attempts.
- [ ] Persist authenticated operator state (`user_id`, `username`, timestamp) in `localStorage` upon `AUTH_OK`.
- [ ] Render authenticated header showing active operator badge and "Sign Out" action button.
- [ ] Unit/component verification testing for login, lockout timer countdown, and session persistence.

## Comments

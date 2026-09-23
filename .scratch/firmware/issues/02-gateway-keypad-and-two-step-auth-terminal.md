# 02: Gateway (Board A) Matrix Keypad & Two-Step Auth Terminal

**What to build:** An end-to-end interactive physical terminal workflow running on the Gateway (Board A). The operator interacts with a 1.3" I2C OLED display and a 4x4 matrix keypad in a two-step authentication flow. In Phase 1, the display prompts for numeric User ID; numeric keypresses append to the ID buffer, `*` backspaces, and `#` advances to Phase 2. In Phase 2, the display prompts for numeric PIN; digits appear in plaintext for visual verification, `*` deletes or returns to Phase 1, and `#` triggers authentication dispatch. The terminal formats the outgoing JSON authentication payload and renders all incoming status responses (`AUTH_OK`, `INVALID_PIN` with attempts remaining, `USER_LOCKED` countdown, and `USER_NOT_FOUND`).

**Blocked by:** 01: Project Scaffolding, Library Configuration, and Protocol Definitions

**Status:** completed

- [x] OLED initializes cleanly on I2C pins without column shifting or graphical corruption.
- [x] Matrix keypad scans keys responsively without key bounce or spurious double-triggers.
- [x] Phase 1 correctly accepts multi-digit User IDs, handles `*` backspacing, and advances to Phase 2 on `#`.
- [x] Phase 2 correctly renders plaintext PIN digits, handles `*` deletion and Phase 1 cancellation, and triggers submission on `#`.
- [x] Submission generates a valid JSON payload string matching `{"user_id": <int>, "pin": "<str>"}`.
- [x] Terminal correctly renders and transitions between `AUTH_OK` greeting (3-second duration), `INVALID_PIN` attempt warning, `USER_LOCKED` active countdown timer, and `USER_NOT_FOUND`.
- [x] Keypad scanning and OLED rendering operate on Core 1 decoupled from network operations via FreeRTOS Queues.


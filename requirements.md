# Software and Hardware Requirements Specification (SRS)

## Distributed Sorting, Counting, and Access Control System

---

## 1. Project Overview and Technology Stack

This project is an end-to-end IoT industrial sorting and tracking simulation. The system captures shape detection events, coordinates physical actuation and binary LED telemetry across two microcontrollers over low-latency wireless links, synchronizes state with an MQTT broker, persists historical logs and user state in a managed cloud database, and provides both local physical authentication and a real-time web monitoring dashboard.

### Core Stack Decisions

- **Database & Persistence:** Supabase (Managed PostgreSQL) for relational storage, row-level updates, audit trails, and user credentials.
- **Backend API & Orchestration:** Go (Golang), hosted as a web service on Render, acting as the secure bridge between external webhooks, the database, and the MQTT network.
- **MQTT Broker:** HiveMQ Cloud (Serverless/Free Tier), handling pub/sub messaging between the backend, the gateway microcontroller, and the client dashboard over secure TLS/WebSocket connections.
- **Firmware Platform:** PlatformIO with the Arduino framework for ESP32 development.
- **Frontend Web Application:** React Single-Page Application (SPA) built with Vite and Tailwind CSS.
- **Repository Layout:**
- `firmware-monorepo`: Houses both PlatformIO project workspaces (`board-a-gateway` and `board-b-actuator`) along with shared C header definitions.
- `backend-service`: Houses the Go API service, database migration scripts, and MQTT worker routines.
- `dashboard-ui`: Houses the React/Vite front-end application (or co-located with the backend as a monorepo workspace).

---

## 2. Microcontroller Subsystem Specifications

### 2.1. Board A: Gateway, Local Authentication, and User Interface

- **Role:** Acts as an edge gateway bridging the cloud layer (Wi-Fi/MQTT) and the field actuation layer (ESP-NOW), while handling physical access terminal operations.
- **Hardware Peripherals:**
- 0.96-inch or 1.3-inch I2C OLED Display (SSD1306/SH1106).
- 4x4 or 4x3 Matrix Keypad for numeric input, user selection, and form submission.

- **Functional Requirements:**
- Connects as a Wi-Fi Station (STA) to a local mobile hotspot network.
- Dynamically queries its assigned Wi-Fi RF channel and establishes an ESP-NOW peer link with Board B on that identical operating frequency.
- Establishes and maintains a secure MQTT client session with HiveMQ Cloud.
- Keypad Handling and Two-Step Authentication Flow:
- The terminal uses a two-phase entry workflow: User Identifier selection followed by PIN submission.
- Uses `#` as the Enter/Confirm key and `*` as the Backspace/Clear key.
- Phase 1 (User Selection): OLED prompts for user identity (e.g., `User ID: _`). The operator inputs a single-digit or multi-digit numeric User ID and presses `#`.
- Phase 2 (PIN Entry): OLED updates to prompt for the password (e.g., `PIN: ____`). The operator enters their plaintext numeric PIN and presses `#`.
- Submitting Phase 2 packages both the selected User ID and the plaintext PIN into a single JSON payload transmitted to the backend over MQTT.

- Display Handling:
- Displays guided prompts for the two-step login procedure.
- Renders live status strings returned by the backend: authentication success greeting with the user's name, invalid attempt warnings with remaining attempt count for that specific user, or an active lockout timer countdown.
- During normal operations, displays current Wi-Fi connection quality, MQTT connectivity status, and a summary of active hardware operations.

- Protocol Bridging:
- Subscribes to backend-originating MQTT topics for incoming shape detections and servo toggle commands, repacking them into binary ESP-NOW packets transmitted to Board B.
- Listens for ESP-NOW telemetry packets transmitted by Board B, immediately translating and publishing them as JSON payloads to corresponding HiveMQ MQTT topics.

### 2.2. Board B: Actuation, Binary Counting, and Hardware State

- **Role:** Manages mechanical actuation, visual binary tally displays, and local safety pause controls without holding direct internet connectivity.
- **Hardware Peripherals:**
- 9 discrete LEDs organized into three distinct color banks: 3 Red LEDs, 3 Green LEDs, 3 Blue LEDs.
- 1 PWM Standard Hobby Servo Motor (0 to 90 degrees operating range).
- 1 DC Gear Motor driven through an external H-Bridge motor driver board.
- 1 Momentary Push Button wired with debounce handling.

- **Functional Requirements:**
- Operates on ESP-NOW wireless protocol locked to Board A’s radio frequency channel.
- LED Binary Counting Representation:
- Shape 1 (Circle) maps exclusively to the 3 Red LEDs.
- Shape 2 (Triangle) maps exclusively to the 3 Green LEDs.
- Shape 3 (Square) maps exclusively to the 3 Blue LEDs.
- Each 3-LED bank represents a binary number from 0 (`000`) up to 5 (`101`), where the first LED is Bit 0 ($2^0$), the second is Bit 1 ($2^1$), and the third is Bit 2 ($2^2$).
- Upon receiving a valid shape detection packet from Board A, the respective shape counter increments by 1 and updates its LED bank.
- When a shape count increments to 5 (`101`), the LEDs must hold the `101` visual state for an intentional 800-millisecond observation delay. After this duration, the internal counter resets to 0, the LED bank turns completely off (`000`), and a rollover batch event payload is transmitted to Board A for database total accumulation.

- Motor and Actuator Operations:
- The DC gear motor runs continuously during active operations to simulate a feeder or conveyor line.
- The servo motor acts as an electro-mechanical sorting gate, changing between a Closed position (0 degrees) and an Open position (90 degrees) based on commands received via ESP-NOW from the dashboard/backend.

- Physical Pause / Resume Interruption:
- Pressing the momentary button toggles the operational state between Active and Paused.
- Entering Paused state immediately cuts drive voltage to the DC motor, blocks any further shape counting increments, and transmits an updated telemetry packet to Board A.
- Exiting Paused state restores drive power to the DC motor, re-enables shape detection processing, and preserves existing binary counts on all LED banks without resetting them.

---

## 3. Backend and API Specifications (Go on Render)

- **Role:** Centralized authority for authentication validation, credential lockout enforcement, detection ingestion from the vision service, and bi-directional MQTT data coordination.
- **External Webhook Ingestion:**
- Exposes a public HTTP POST endpoint (`/api/v1/detections`) that accepts incoming JSON payloads from the external Vision API.
- Validates bearer authorization tokens to discard unauthorized submissions.
- Maps incoming shape names (`circle`, `triangle`, `square`) to internal numeric IDs (1, 2, 3).
- Enforces an ingestion debounce window to drop accidental multi-trigger submissions occurring under 2 seconds for identical shapes.
- Translates valid events into an MQTT message published to the HiveMQ broker targeting Board A.

- **Authentication Engine & Per-User Lockout Logic:**
- Validates submitted User ID and plaintext PIN pairs received via MQTT (from Board A's keypad) or via HTTP REST (from the React dashboard login screen).
- Performs direct plaintext equality checks between the submitted PIN and the `pin` column stored for that specific user in Supabase.
- Evaluates lockout state on a per-user basis:
- If the specified user record has an active `locked_until` timestamp that is greater than the current time, the authentication attempt is rejected immediately with a lockout status and the remaining wait duration.
- If the user exists and the submitted PIN does not match, the backend increments that user's `failed_attempts` counter.
- If `failed_attempts` reaches 2, the backend sets `locked_until` to 60 seconds into the future and returns a locked status.
- If the user exists and the submitted PIN matches, the backend resets `failed_attempts` to 0, clears `locked_until`, and returns an authentication success payload containing the user's display name.
- If the submitted User ID does not exist in the database, the backend returns an unknown user error without affecting existing user lockout records.

- **Telemetry and Persistence Synchronization:**
- Runs an internal MQTT background worker that continuously listens to telemetry topics published by Board A.
- Increments lifetime cumulative counters in Supabase whenever a shape batch rollover occurs.
- Updates peripheral state records in Supabase (motor status, servo position, pause state) to provide a single source of truth.

---

## 4. Database Specifications (Supabase / PostgreSQL)

### 4.1. Entity Relationships and Data Tables

- **Users and Plaintext Credentials Table (`users`):**
- `id`: Integer primary key (serves as the numeric User ID entered on the keypad, e.g., 1, 2, 3).
- `username`: Human-readable identifier for dashboard display and OLED welcome screens (e.g., "Alice", "Bob").
- `pin`: Plaintext numeric string storing the user's password (e.g., "1234").
- `failed_attempts`: Integer counter tracking consecutive failed logins for this specific user (0, 1, or 2).
- `locked_until`: Nullable timestamp recording the expiration time of active security lockouts.
- `created_at`: Account creation timestamp.

- **Hardware System State Table (`system_state`):**
- Enforced singleton record storing current hardware telemetry.
- `is_paused`: Boolean indicating whether Board B is halted.
- `motor_state`: Boolean indicating whether the DC gear motor is active.
- `servo_state`: Boolean indicating whether the gate servo is open or closed.
- `last_telemetry_at`: Timestamp of the latest state sync.

- **Shape Counters Table (`shape_counts`):**
- Three persistent records corresponding to Circle, Triangle, and Square.
- `shape_id`: Primary key integer (1, 2, 3).
- `shape_name`: String identifier (`circle`, `triangle`, `square`).
- `color_label`: String identifier (`red`, `green`, `blue`).
- `live_buffer`: Integer representing the current unrolled count on Board B (0 through 5).
- `total_lifetime`: BigInt recording the total accumulated shape count over the system's operational history.
- `updated_at`: Timestamp of last increment.

- **Access Control Audit Table (`auth_audit_logs`):**
- Append-only record tracking all authentication activity across both input surfaces.
- `id`: Unique UUID.
- `source`: Enum (`KEYPAD` or `DASHBOARD`).
- `user_id`: Nullable integer foreign key referencing the attempted User ID.
- `status`: Enum (`SUCCESS`, `INVALID_PIN`, `USER_LOCKED`, `USER_NOT_FOUND`).
- `timestamp`: Creation timestamp.

---

## 5. Web Dashboard Specifications (React + Vite)

- **Role:** Operator interface providing live observational telemetry, remote actuation controls, and role-authenticated views.
- **Authentication View:**
- Displays a user identity selector (dropdown list or user cards showing available users like Alice, Bob, etc.) alongside a numeric PIN input field.
- Allows operators to log in using the exact same numeric User ID and plaintext PIN configured for Board A.
- Displays specific feedback targeting the chosen user: remaining attempts allowed or an active lockout countdown timer if that specific user has exceeded 2 failed attempts.

- **Operational Control View:**
- **System Status Indicator:** Real-time badge reflecting whether Board B is currently running or paused via the physical push button.
- **Servo Gate Actuator Control:** Interactive manual toggle switch allowing authenticated operators to command the physical servo gate between Open and Closed states. Emits commands to the Go backend / HiveMQ topic.
- **Motor Indicator:** Visual indicator showing whether drive power to the DC motor is energized.
- **Shape Telemetry Cards (3 Segments):**
- Red Card (Circle): Visual 3-bit binary indicator displaying live bits along with the current decimal value (0 to 5) and the cumulative lifetime database total.
- Green Card (Triangle): Visual 3-bit binary indicator displaying live bits along with the current decimal value (0 to 5) and the cumulative lifetime database total.
- Blue Card (Square): Visual 3-bit binary indicator displaying live bits along with the current decimal value (0 to 5) and the cumulative lifetime database total.

- **Real-Time Data Layer:**
- Subscribes directly to HiveMQ over WebSockets (or uses Supabase Realtime subscriptions) to guarantee sub-second visual updates without full page reloads.

---

## 6. End-to-End Operational Workflows

### 6.1. Shape Detection and Binary Rolling Count

1. An external computer vision service captures a shape and issues an authenticated HTTP POST webhook to the Go backend on Render.
2. The Go backend verifies the payload, deduplicates the event, maps the shape string to an integer ID, and publishes an MQTT message to HiveMQ.
3. Board A, subscribed to HiveMQ, receives the detection message and retransmits it immediately as a packed binary payload over ESP-NOW to Board B.
4. Board B receives the packet, validates that the system is not paused, increments the corresponding shape's counter, and updates the respective 3-LED bank in binary.
5. When a count transitions from 4 to 5, the LED bank shows binary `101`. Board B holds this state for 800 milliseconds, resets the count to 0, turns off the LEDs (`000`), and sends an ESP-NOW batch completion notification back to Board A.
6. Board A relays the completion message to HiveMQ, where the Go backend receives it and updates both the live buffer and cumulative totals in Supabase.
7. The React dashboard reflects both the live binary cycle and the persistent database count.

### 6.2. Two-Step Keypad Authentication and Per-User Lockout

1. A user approaches Board A. The OLED display reads: `Select User ID: _`.
2. The user enters their numeric identifier (e.g., `1`) and presses `#`.
3. The OLED updates to prompt for credentials: `User 1 PIN: ____`.
4. The user enters their plaintext PIN (e.g., `1234`) and presses `#`.
5. Board A packages the credentials into a JSON payload containing `user_id` and `pin`, publishing it to the HiveMQ topic `factory/auth/request`.
6. The Go backend processes the message and queries the `users` table in Supabase for the specified `id`:

- If the user is currently locked (`locked_until` is active): The backend replies with a `USER_LOCKED` status and remaining seconds. Board A renders: `User Locked! Wait: Xs`.
- If the user exists but the plaintext PIN does not match: The backend increments `failed_attempts` on that user's record. If `failed_attempts` reaches 2, `locked_until` is stamped with a 60-second expiration. The backend replies with `INVALID_PIN`. Board A renders: `Wrong PIN! Attempts: 1` or `User Locked! Wait: 60s`.
- If the user exists and the plaintext PIN matches: The backend resets `failed_attempts` to 0, clears any lockout timestamp, logs the event in `auth_audit_logs`, and replies with `AUTH_OK` and the user's `username`. Board A renders: `Welcome, Alice!` for 3 seconds before returning to the default operational display.

### 6.3. Remote Actuation and Push Button Pausing

1. When an authenticated operator toggles the Servo switch on the React dashboard, an update is dispatched through the Go backend to HiveMQ.
2. Board A intercepts the MQTT toggle instruction and relays an actuation packet via ESP-NOW to Board B.
3. Board B sets the PWM signal to the servo motor (0 degrees for Closed, 90 degrees for Open), stores the local state, and transmits an updated telemetry confirmation.
4. If a floor operator presses the physical push button on Board B at any time:

- Board B toggles its internal pause flag.
- If transitioning to paused: Motor drive pins are driven LOW to stop rotation, incoming shape events are ignored, and an emergency status packet is dispatched over ESP-NOW.
- Board A forwards the status to HiveMQ.
- The Go backend updates the `system_state` table in Supabase.
- The React dashboard updates its primary status banner to display an alert indicating operations are paused at the machine level.

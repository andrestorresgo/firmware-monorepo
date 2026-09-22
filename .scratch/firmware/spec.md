Status: ready-for-agent

# Firmware Subsystem Specification

## Problem Statement

The industrial sorting and tracking simulation requires an embedded field deployment that coordinates physical sorting actuation, binary LED status telemetry, user authentication, and cloud connectivity across multiple microcontrollers. Currently, the firmware monorepo contains only empty boilerplate PlatformIO projects. Without functional firmware, physical hardware actuation (conveyor line, sorting gate, shape counting displays, pause safety toggle) and access control (keypad entry and OLED feedback) cannot operate or synchronize with the backend service and cloud MQTT broker.

## Solution

Build two independent, production-grade firmware projects for ESP32 microcontrollers within the monorepo:
1. **Gateway (`Board A`)**: Connects to the local mobile hotspot Wi-Fi, manages a secure TLS MQTT session with HiveMQ Cloud, dynamically bridges MQTT events to ESP-NOW wireless packets, and manages a physical Two-Step Authentication terminal via an I2C OLED display and a 4x4 matrix keypad.
2. **Actuator (`Board B`)**: Operates on ESP-NOW locked to the Gateway's RF channel, executes real-time physical actuation (continuous DC conveyor motor via an abstract H-Bridge driver, 0°/90° servo sorting gate), displays 3-bit binary shape counts across 9 discrete LEDs with an 800ms Observation Delay and Batch Rollover mechanism, and enforces Machine Pause with total hardware lockout via a momentary safety push button.

## User Stories

1. As an operator at the physical machine terminal, I want the Gateway OLED to display a prompt for my numeric User ID, so that I can initiate the Two-Step Authentication flow.
2. As an operator entering my User ID, I want each numeric keypress to appear immediately on the OLED, so that I can verify my input before confirming.
3. As an operator entering my User ID, I want the `*` key to act as backspace, so that I can correct mistaken digit entries.
4. As an operator who has typed my User ID, I want the `#` key to advance to PIN entry, so that I can proceed to the second authentication step.
5. As an operator entering my PIN, I want the OLED to display the entered digits in plaintext, so that I can easily visually debug and verify the credential during simulation testing.
6. As an operator entering my PIN, I want the `*` key to delete the last entered digit or return to the User ID step if empty, so that I can abort or correct my entry.
7. As an operator submitting my PIN with `#`, I want the Gateway to immediately package my User ID and PIN into a JSON payload and publish it to `factory/auth/request`, so that the backend service can validate my credentials.
8. As an operator waiting for authentication, I want the Gateway OLED to show a verification indicator, so that I know the system is communicating with the cloud.
9. As an authorized operator whose login succeeds, I want the Gateway OLED to show a greeting with my username (e.g., `Welcome, Alice!`) for 3 seconds before returning to the idle screen, so that I receive positive confirmation of access.
10. As an operator who enters an incorrect PIN, I want the Gateway OLED to display an invalid PIN warning alongside my remaining attempt count, so that I am informed before being locked out.
11. As an operator who exceeds allowed attempts, I want the Gateway OLED to display a locked status with an active countdown timer, so that I know exactly how long I must wait.
12. As an operator approaching an offline Gateway, I want the OLED to clearly indicate network connection status and block auth submissions, so that I do not attempt to log in while the cloud is unreachable.
13. As a field operator, I want the Gateway to dynamically discover its Wi-Fi channel from the mobile hotspot and broadcast it, so that the Actuator can synchronize without manual network reconfiguration.
14. As an Actuator microcontroller booting in the field, I want to automatically sweep 2.4 GHz channels 1 through 13 to detect the Gateway's discovery beacon, so that I can lock my radio frequency and establish ESP-NOW communication automatically.
15. As the vision detection system, I want valid shape detections forwarded by the cloud to reach the Actuator over low-latency ESP-NOW, so that physical counters reflect detected objects in real time.
16. As an observer watching the Actuator, I want Shape 1 (Circle) detections to increment the 3 Red LEDs in binary ($2^0, 2^1, 2^2$), so that I can visually verify Circle counts from 0 (`000`) to 5 (`101`).
17. As an observer watching the Actuator, I want Shape 2 (Triangle) detections to increment the 3 Green LEDs in binary ($2^0, 2^1, 2^2$), so that I can visually verify Triangle counts from 0 (`000`) to 5 (`101`).
18. As an observer watching the Actuator, I want Shape 3 (Square) detections to increment the 3 Blue LEDs in binary ($2^0, 2^1, 2^2$), so that I can visually verify Square counts from 0 (`000`) to 5 (`101`).
19. As an observer watching a shape counter reach 5 (`101`), I want the LEDs to maintain the `101` visual state for an intentional 800ms Observation Delay, so that human observers can register the completed cycle.
20. As an automated sorting system, I want any duplicate detection for the same shape arriving during the 800ms Observation Delay to be safely dropped, so that counting errors and race conditions are prevented.
21. As a cloud database supervisor, I want the Actuator to reset the LEDs to `000` after the Observation Delay and immediately dispatch a Batch Rollover packet to the Gateway, so that cumulative lifetime totals can be persisted in the database.
22. As the sorting process line, I want the DC conveyor motor to run continuously at a stable duty cycle (80% default) during normal operation, so that parts are fed along the sorting line.
23. As an operator or remote supervisor, I want servo gate open/close commands from the dashboard to actuate the sorting gate between 0° (Closed) and 90° (Open) over ESP-NOW, so that parts can be physically routed.
24. As a machine operator in an abnormal or hazard situation, I want pressing the physical push button on the Actuator to immediately trigger Machine Pause, so that all physical actuation stops instantly.
25. As a safety supervisor, I want Machine Pause to enforce total hardware lockout (cutting drive voltage to the DC conveyor motor and locking the servo sorting gate against movement), so that the machine remains strictly safe.
26. As an operator resuming operation after a pause, I want pressing the push button again to restore DC motor power, unlock actuation, and re-enable shape increments while preserving existing binary counts on all LED banks, so that work can resume without loss of progress.
27. As a web dashboard user, I want the Actuator to emit an immediate Telemetry Heartbeat upon any state change (pause toggle, servo transition, counter increment) and periodically every 3 seconds, so that the live monitoring screen is constantly updated.

## Implementation Decisions

### Monorepo and Project Isolation
- Both `board-a-gateway` and `board-b-actuator` are standalone PlatformIO projects targeting the ESP32 platform using the Arduino framework, with no shared compiler include paths (per ADR-0001).
- Each project maintains a local protocol definition header specifying identical binary packet structures, magic bytes, and message types.
- Network credentials and broker secrets are isolated in git-ignored `secrets.h` header files with checked-in `secrets.h.example` templates.

### Hardware Mapping and Drivers
- **Gateway (`Board A`)**:
  - Display: 1.3" DST-013 OLED driven by SH1106 controller over I2C on `GPIO 21` (SDA) and `GPIO 22` (SCL) using the `U8g2` library.
  - Matrix Keypad: 4x4 keypad using rows `GPIO 13, 12, 14, 27` and columns `GPIO 26, 25, 33, 32` via the `Keypad` library.
- **Actuator (`Board B`)**:
  - LED Banks: 9 discrete LEDs wired in Active-HIGH configuration (GPIO $\rightarrow$ Resistor $\rightarrow$ Anode, Cathode to GND):
    - Red LEDs (Circle, Bits 0, 1, 2): `GPIO 15, 2, 4`
    - Green LEDs (Triangle, Bits 0, 1, 2): `GPIO 16, 17, 5`
    - Blue LEDs (Square, Bits 0, 1, 2): `GPIO 18, 19, 21`
  - Servo Gate: Driven by `ESP32Servo` on `GPIO 13` (0° for Closed, 90° for Open).
  - DC Conveyor Motor: Managed via an abstract `HBridgeMotor` class utilizing ESP32 `ledc` PWM on `GPIO 22` (IN1) with a default 80% duty cycle (204/255) and static direction on `GPIO 23` (IN2).
  - Safety Push Button: Wired on `GPIO 25` with internal pullup (`INPUT_PULLUP`, Active-LOW) and 50ms software debounce.

### Concurrency and Core Affinity
- Microcontroller tasks are scheduled across both ESP32 cores using native FreeRTOS (per ADR-0003):
  - **Core 0**: Dedicated to networking and wireless communication (TLS MQTT via `WiFiClientSecure` and ESP-NOW on Board A; ESP-NOW transport and channel scanning on Board B).
  - **Core 1**: Dedicated to real-time hardware scanning and UI execution (Keypad polling and OLED rendering on Board A; LED timers, motor PWM, servo movement, and button interrupt servicing on Board B).
- Data sharing between cores occurs strictly through thread-safe FreeRTOS Queues.

### Wireless Protocol & State Synchronization
- **ESP-NOW Pairing**: Board A queries its Wi-Fi STA channel and broadcasts discovery beacons. Board B sweeps channels 1 to 13 on boot until detecting Board A, locking its radio channel and saving the peer MAC (per ADR-0002).
- **Packet Validation**: All inter-board binary frames begin with a magic byte `0xA5` followed by a packet type discriminator.
- **Telemetry and Heartbeat**: Board B broadcasts telemetry packets containing machine pause status, motor state, servo position, and live binary counts upon any state change and every 3 seconds as a heartbeat.
- **Machine Pause Enforcement**: Pressing the push button toggles Machine Pause, enforcing complete hardware lockout of the DC motor and servo gate while blocking shape count processing (per ADR-0004).
- **Rollover Sequence**: Incrementing to 5 (`101`) triggers an 800ms non-blocking software timer during which subsequent detections for that shape are dropped. Once expired, the bank clears to `000` and a Batch Rollover frame is transmitted.

### MQTT Protocol & Schema
- Board A publishes auth requests to `factory/auth/request`: `{"user_id": <int>, "pin": "<str>"}`.
- Board A receives auth responses from `factory/auth/response`: `{"status": "<enum>", "username": "<str>", "remaining_attempts": <int>, "lockout_seconds": <int>}`.
- Board A receives shape detections on `factory/detections`: `{"shape_id": <int>, "shape_name": "<str>"}`.
- Board A receives actuator gate commands on `factory/actuator/servo`: `{"state": "OPEN"|"CLOSED"}`.
- Board A publishes live hardware telemetry on `factory/telemetry`: `{"is_paused": <bool>, "motor_state": <bool>, "servo_state": <bool>, "red_count": <int>, "green_count": <int>, "blue_count": <int>}`.
- Board A publishes rollover notifications on `factory/rollover`: `{"shape_id": <int>, "shape_name": "<str>", "timestamp": <int>}`.

## Testing Decisions

### What Makes a Good Test
- Tests must verify external behavioral contracts, state transitions, and protocol framing rather than hardware-specific register manipulation.
- State machines and binary protocol serialization must be testable independently of physical hardware by decoupling logic engines from hardware abstraction interfaces.

### Modules to Test
1. **Protocol Serialization and Deserialization Engine**:
   - Verify that binary ESP-NOW structs serialize and deserialize accurately with matching magic byte `0xA5` across both boards.
   - Verify that corrupted frames or invalid magic bytes are rejected without state mutation.
2. **Actuator Counting & Rollover State Engine**:
   - Verify that sequential shape detections increment the corresponding shape counter through binary values 0 to 5.
   - Verify that reaching count 5 enters the Observation Delay state and triggers a Batch Rollover event with count reset to 0 upon completion.
   - Verify that duplicate shape detections received while in Observation Delay are dropped.
3. **Machine Pause State Engine**:
   - Verify that toggling Machine Pause transitions motor and servo states to locked.
   - Verify that shape detection events are rejected during Machine Pause.
   - Verify that exiting Machine Pause restores motor state and preserves existing LED shape counts.
4. **Gateway Keypad & Authentication State Engine**:
   - Verify that numeric keypresses append to User ID in Phase 1 and PIN in Phase 2.
   - Verify that backspace removes characters and cancels back to Phase 1 when empty.
   - Verify that submitting in Phase 2 formats the correct JSON payload for MQTT transmission.
   - Verify that receiving `AUTH_OK`, `INVALID_PIN`, and `USER_LOCKED` transitions the display state appropriately.

### Prior Art
- Standard PlatformIO unit testing (`pio test`) with native test environment (`env:native` using desktop GCC/Clang) and mock hardware wrappers.

## Out of Scope
- Backend Go API implementation and Supabase database schema migrations (handled in `backend-service`).
- Computer Vision shape classification and HTTP webhook dispatching (handled by external vision service).
- React/Vite web monitoring dashboard implementation (handled in `dashboard-ui`).
- Dynamic OTA (Over-The-Air) firmware flashing over Wi-Fi.

## Further Notes
- Pin definitions for Board B utilize ESP32 strapping pins (`GPIO 2` and `GPIO 15`) for LED bank bits; breadboard wiring must ensure these lines are not externally forced to invalid boot levels during hardware reset.

# Distributed Sorting, Counting, and Access Control System

Industrial IoT simulation coordinating physical actuation, shape detection tracking, binary telemetry, and cloud synchronization across microcontrollers and web services.

## Language

**Gateway (`Board A`)**:
The edge microcontroller bridging the HiveMQ Cloud MQTT broker and the local ESP-NOW wireless mesh, handling physical access authentication and user prompts.
_Avoid_: Base station, controller A, master, router.

**Actuator (`Board B`)**:
The field microcontroller executing physical actuation (DC motor, servo gate), managing 3-bit binary visual counters via LED banks, and handling hardware pause without direct internet connectivity.
_Avoid_: Worker, slave, node, controller B.

**Shape Counter**:
The local 3-bit binary accumulation (values 0 through 5) represented on a dedicated 3-LED bank on the Actuator for each supported geometric shape.
_Avoid_: Shape tally, shape buffer, shape register.

**Observation Delay**:
The intentional 800-millisecond visual hold period on the Actuator when a shape counter reaches binary `101` (5) before automatically resetting to `000` (0).
_Avoid_: Linger time, pause duration, rollover sleep, hold window.

**Batch Rollover**:
The event triggered immediately following the observation delay when a shape counter transitions from 5 to 0, publishing cumulative batch totals to the Gateway.
_Avoid_: Overflow, reset event, cycle complete.

**Two-Step Authentication**:
The physical terminal access control workflow requiring sequential numeric User ID submission followed by plaintext numeric PIN entry on the Gateway's matrix keypad.
_Avoid_: 2FA, multi-factor auth, double login, keycard auth.

**Machine Pause**:
The hardware-level operational state toggled by the physical momentary push button on the Actuator, resulting in total physical actuation lockout (cutting power to the DC motor and locking the servo gate) while blocking shape counting.
_Avoid_: Emergency stop, soft pause, system standby, motor kill.

**Telemetry Heartbeat**:
The hybrid periodic (3-second interval) and event-driven status frame transmitted from the Actuator to the Gateway over ESP-NOW containing live peripheral states and current binary counter values.
_Avoid_: Polling packet, ping, keepalive frame.

**Per-User Lockout**:
The temporary 60-second security freeze applied strictly to an individual user record after 2 consecutive failed PIN attempts, without impacting other users.
_Avoid_: System lockout, IP ban, global lockout.

**Ingestion Debounce Window**:
The 2-second rate-limiting window enforced on incoming Vision API webhooks to drop duplicate detections of the same shape.
_Avoid_: Throttle delay, camera sleep, detection cooldown.

**Access Audit Trail**:
The immutable, append-only log record (`auth_audit_logs`) tracking authentication attempts across both input surfaces (`KEYPAD` and `DASHBOARD`).
_Avoid_: Login history, session table, event log.

**Singleton System State**:
The single-row database record (`system_state`) reflecting current physical hardware telemetry (pause status, DC motor drive state, servo gate angle) synchronized from Board A's MQTT telemetry heartbeat.
_Avoid_: System cache, device config, telemetry table.

**Operator Session**:
The client-side authenticated state of an operator on the web dashboard authorizing remote actuation commands.
_Avoid_: User token, web login state, operator profile.

**Binary Bit Indicator**:
The visual 3-element display ($2^2, 2^1, 2^0$) representing the live physical LED states of a Shape Counter on the dashboard.
_Avoid_: LED widget, binary badge, bit array.



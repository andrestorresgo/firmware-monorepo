# 06: Actuator (Board B) Physical Actuation & Machine Pause Safety Lockout

**What to build:** Physical actuation and safety halt logic on the Actuator (Board B). The Actuator continuously drives the DC conveyor motor via the abstract `HBridgeMotor` class on `GPIO 22` and `GPIO 23` using ESP32 `ledc` PWM at an 80% duty cycle. Incoming servo commands routed from MQTT `factory/actuator/servo` through the Gateway actuate the sorting gate between 0° (Closed) and 90° (Open). A momentary push button on `GPIO 25` with internal pullup toggles Machine Pause. Entering Machine Pause enforces total hardware lockout (immediately cutting DC motor PWM to 0% and locking servo movement) and blocks shape counting increments. Exiting Machine Pause re-energizes the DC motor and re-enables counting while preserving existing binary counts on all LED banks. State transitions trigger immediate event-driven telemetry packets to the Gateway.

**Blocked by:** 05: Actuator (Board B) Binary LED Counting, Observation Delay, and Batch Rollover

**Status:** ready-for-agent

- [ ] Abstract `HBridgeMotor` class cleanly drives motor forward with 80% duty cycle via ESP32 `ledc`.
- [ ] Gateway subscribes to `factory/actuator/servo` and forwards commands (`OPEN` / `CLOSED`) over ESP-NOW.
- [ ] Actuator moves the servo gate between 0° and 90° via `ESP32Servo` on `GPIO 13` upon command.
- [ ] Push button on `GPIO 25` toggles Machine Pause state with a 50ms software debounce.
- [ ] Entering Machine Pause immediately cuts DC motor drive to 0 and suppresses servo gate commands.
- [ ] Entering Machine Pause blocks shape detection increments.
- [ ] Exiting Machine Pause restores DC motor power, unlocks gate commands, and preserves LED binary counts.
- [ ] Every transition of the pause button or servo gate emits an immediate telemetry frame over ESP-NOW to update the cloud.

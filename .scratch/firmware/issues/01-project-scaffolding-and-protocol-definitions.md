# 01: Project Scaffolding, Library Configuration, and Protocol Definitions

**What to build:** The foundational build and dependency setup for both `board-a-gateway` and `board-b-actuator` standalone projects. Both PlatformIO project workspaces must resolve required external libraries (graphics, keypad, MQTT, servo, JSON serialization) and compiler settings cleanly. Both targets must include local binary protocol definitions encoding packet framing (with magic byte `0xA5`), message opcodes, and data payloads. Secrets configuration templates must be established to keep sensitive credentials out of version control, and native unit testing harnesses must be configured for headless validation.

**Blocked by:** None (can start immediately)

**Status:** ready-for-agent

- [ ] Both `board-a-gateway` and `board-b-actuator` build successfully in PlatformIO with zero compilation errors.
- [ ] Dependencies (`U8g2`, `Keypad`, `PubSubClient`, `ESP32Servo`, `ArduinoJson`) are properly specified and fetched.
- [ ] Protocol definitions exist in each project defining identical binary structs for beacons, commands, telemetry, and batch rollover with frame validation (`0xA5`).
- [ ] A `secrets.h.example` template is provided and real `secrets.h` files are excluded by `.gitignore`.
- [ ] Unit test runners are configured to allow logic and protocol serialization testing without physical hardware.

# 04: Actuator (Board B) Dynamic RF Channel Discovery & ESP-NOW Link

**What to build:** Automatic RF channel synchronization and bidirectional wireless link on the Actuator (Board B). When booting, the Actuator initializes ESP-NOW on Core 0 and sweeps 2.4 GHz channels 1 through 13 until it captures a discovery beacon broadcast by the Gateway. The Actuator sets its radio frequency to the discovered channel, registers the Gateway's MAC address as a peer, and replies with a pairing acknowledgment. Once paired, the Actuator transmits hybrid Telemetry Heartbeats (periodic every 3 seconds, plus event-driven upon state changes) containing machine pause state, motor status, servo position, and shape counter values. The Gateway forwards received telemetry frames to HiveMQ topic `factory/telemetry`.

**Blocked by:** 01: Project Scaffolding, Library Configuration, and Protocol Definitions, 03: Gateway (Board A) Cloud Connectivity and Channel Beacon

**Status:** completed

- [x] Actuator automatically sweeps channels 1 to 13 on boot and detects the Gateway's beacon within 2 seconds.
- [x] Actuator locks its radio to the discovered channel and registers the Gateway peer successfully.
- [x] Bidirectional ESP-NOW packets pass frame validation using magic byte `0xA5`.
- [x] Actuator emits Telemetry Heartbeats every 3 seconds over ESP-NOW.
- [x] Gateway receives telemetry frames on Core 0 and publishes JSON payloads to `factory/telemetry`.
- [x] Disconnections or channel loss trigger automatic re-scanning on the Actuator without locking up the MCU.


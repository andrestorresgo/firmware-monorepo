# 03: Gateway (Board A) Cloud Connectivity and Channel Beacon

**What to build:** Cloud networking and RF beacon broadcasting on the Gateway (Board A). The Gateway connects as a Wi-Fi Station (STA) to a local mobile hotspot, retrieves its assigned 2.4 GHz RF operating channel, and establishes a secure TLS MQTT connection with HiveMQ Cloud on Core 0. The Gateway publishes authentication requests to `factory/auth/request` and routes responses from `factory/auth/response` to the terminal display. The OLED renders a real-time connection status indicator and enforces strict offline lockout of auth entry when disconnected. Once connected to Wi-Fi, the Gateway continuously broadcasts discovery beacons containing its RF channel and MAC address over ESP-NOW to enable the Actuator to pair.

**Blocked by:** 02: Gateway (Board A) Matrix Keypad & Two-Step Auth Terminal

**Status:** completed

- [x] Gateway connects reliably to Wi-Fi Station mode using credentials from `secrets.h`.
- [x] TLS MQTT client establishes and maintains an authenticated connection to HiveMQ Cloud over port 8883.
- [x] Network status updates are posted to the UI task via FreeRTOS queue and rendered on the OLED.
- [x] If disconnected from Wi-Fi or MQTT, the OLED displays an offline warning and blocks auth submission.
- [x] Outgoing auth requests from the terminal are published to `factory/auth/request`.
- [x] Incoming messages on `factory/auth/response` are parsed and delivered to the terminal state engine.
- [x] Wi-Fi RF channel is dynamically queried, and ESP-NOW discovery beacons are broadcast on that channel.

# 05: Actuator (Board B) Binary LED Counting, Observation Delay, and Batch Rollover

**What to build:** The complete shape detection and counting pipeline on the Actuator (Board B). When the Gateway receives shape detection messages on MQTT `factory/detections`, it repacks the event into an ESP-NOW command frame and transmits it to the Actuator. The Actuator increments the respective shape counter (1=Circle/Red, 2=Triangle/Green, 3=Square/Blue) and illuminates the 3-LED bank in binary ($2^0, 2^1, 2^2$). When a counter increments to 5 (`101`), the Actuator enters an 800ms Observation Delay, holds the visual `101` pattern, drops any new detection events for that specific shape during this window, resets the counter and LEDs to `000` upon timer expiration, and transmits a Batch Rollover frame to the Gateway. The Gateway relays the rollover event to MQTT topic `factory/rollover`.

**Blocked by:** 04: Actuator (Board B) Dynamic RF Channel Discovery & ESP-NOW Link

**Status:** ready-for-agent

- [x] Gateway subscribes to `factory/detections` and transmits shape detection frames over ESP-NOW.
- [x] Actuator accurately maps shapes to their respective 3-LED banks (Red=Circle, Green=Triangle, Blue=Square).
- [x] Each bank accurately displays binary values from 0 (`000`) up to 5 (`101`) in Active-HIGH configuration.
- [x] Reaching count 5 engages the 800ms non-blocking Observation Delay hold.
- [x] Duplicate detections for the same shape received during the 800ms Observation Delay are safely dropped.
- [x] Upon expiration of the 800ms window, the bank turns off (`000`), the counter resets to 0, and a Batch Rollover frame is sent to the Gateway.
- [x] Gateway receives the Batch Rollover frame and immediately publishes a JSON payload to `factory/rollover`.

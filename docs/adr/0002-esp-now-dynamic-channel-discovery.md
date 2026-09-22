# Dynamic ESP-NOW Channel Discovery and Pairing

We decided to implement dynamic RF channel scanning and broadcast handshake for the ESP-NOW link between the Gateway (Board A) and Actuator (Board B) instead of hardcoding a static Wi-Fi channel. Because the Gateway operates as a Wi-Fi Station whose channel is dynamically determined by the mobile hotspot, the Actuator sweeps channels 1 to 13 on startup to acquire the Gateway's operating frequency and register peer MAC addresses automatically.

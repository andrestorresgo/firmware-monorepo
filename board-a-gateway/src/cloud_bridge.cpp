#ifndef UNIT_TEST

#include "cloud_bridge.h"
#include <Arduino.h>

CloudBridge* CloudBridge::s_instance = nullptr;

CloudBridge::CloudBridge(QueueHandle_t auth_req_queue,
                         QueueHandle_t auth_resp_queue,
                         QueueHandle_t net_status_queue)
    : auth_req_queue_(auth_req_queue),
      auth_resp_queue_(auth_resp_queue),
      net_status_queue_(net_status_queue),
      mqtt_client_(wifi_client_),
      wifi_connected_(false),
      mqtt_connected_(false),
      wifi_channel_(0),
      esp_now_initialized_(false),
      last_beacon_ms_(0),
      last_mqtt_reconnect_ms_(0),
      last_wifi_check_ms_(0),
      last_status_publish_ms_(0) {
    memset(mac_address_, 0, sizeof(mac_address_));
    s_instance = this;
}

void CloudBridge::begin() {
    Serial.println(F("[CloudBridge] Initializing Wi-Fi Station mode..."));
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    Serial.printf("[CloudBridge] Connecting to Wi-Fi SSID: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Configure TLS Certificate for HiveMQ Cloud
    wifi_client_.setCACert(HIVEMQ_ROOT_CA);

    // Configure MQTT Client
    mqtt_client_.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    mqtt_client_.setCallback(CloudBridge::mqtt_callback);
    mqtt_client_.setBufferSize(512);

    publish_network_status(true);
}

void CloudBridge::loop() {
    check_wifi();
    check_mqtt();
    broadcast_beacon();
    process_outgoing_auth();
    publish_network_status(false);
}

bool CloudBridge::is_wifi_connected() const {
    return wifi_connected_;
}

bool CloudBridge::is_mqtt_connected() const {
    return mqtt_connected_;
}

bool CloudBridge::is_online() const {
    return wifi_connected_ && mqtt_connected_;
}

uint8_t CloudBridge::get_wifi_channel() const {
    return wifi_channel_;
}

void CloudBridge::check_wifi() {
    uint32_t now = millis();
    if (now - last_wifi_check_ms_ < 200) {
        return;
    }
    last_wifi_check_ms_ = now;

    bool current_status = (WiFi.status() == WL_CONNECTED);

    if (current_status && !wifi_connected_) {
        // Just connected to Wi-Fi AP
        wifi_connected_ = true;
        wifi_channel_ = (uint8_t)WiFi.channel();
        WiFi.macAddress(mac_address_);

        Serial.printf("[CloudBridge] Wi-Fi Connected! IP: %s, Channel: %d, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), wifi_channel_, WiFi.RSSI());
        Serial.printf("[CloudBridge] Gateway MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac_address_[0], mac_address_[1], mac_address_[2],
                      mac_address_[3], mac_address_[4], mac_address_[5]);

        setup_esp_now();
        publish_network_status(true);
    } else if (!current_status && wifi_connected_) {
        // Disconnected from Wi-Fi
        wifi_connected_ = false;
        mqtt_connected_ = false;
        Serial.println(F("[CloudBridge] Wi-Fi Connection lost!"));
        publish_network_status(true);
    }
}

void CloudBridge::setup_esp_now() {
    if (!esp_now_initialized_) {
        if (esp_now_init() == ESP_OK) {
            esp_now_initialized_ = true;
            esp_now_peer_info_t peer = {};
            memset(peer.peer_addr, 0xFF, 6); // Broadcast MAC
            peer.channel = wifi_channel_;
            peer.encrypt = false;
            peer.ifidx = WIFI_IF_STA;

            if (esp_now_add_peer(&peer) == ESP_OK) {
                Serial.printf("[CloudBridge] ESP-NOW broadcast peer registered on channel %d\n", wifi_channel_);
            } else {
                Serial.println(F("[CloudBridge] WARN: Failed to add ESP-NOW broadcast peer"));
            }
        } else {
            Serial.println(F("[CloudBridge] ERR: esp_now_init() failed!"));
        }
    } else {
        // Update peer operating channel to match active Wi-Fi channel
        esp_now_peer_info_t peer = {};
        memset(peer.peer_addr, 0xFF, 6);
        peer.channel = wifi_channel_;
        peer.encrypt = false;
        peer.ifidx = WIFI_IF_STA;
        esp_now_mod_peer(&peer);
    }
}

void CloudBridge::check_mqtt() {
    if (!wifi_connected_) {
        return;
    }

    if (mqtt_client_.connected()) {
        mqtt_client_.loop();
    } else {
        if (mqtt_connected_) {
            mqtt_connected_ = false;
            Serial.println(F("[CloudBridge] MQTT connection lost!"));
            publish_network_status(true);
        }

        uint32_t now = millis();
        if (now - last_mqtt_reconnect_ms_ >= 3000) {
            last_mqtt_reconnect_ms_ = now;
            Serial.printf("[CloudBridge] Connecting to HiveMQ Cloud TLS broker (%s:%d)...\n",
                          MQTT_BROKER_HOST, MQTT_BROKER_PORT);

            if (mqtt_client_.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
                Serial.println(F("[CloudBridge] MQTT Connected successfully!"));
                mqtt_connected_ = true;

                // Subscribe to authentication response topic
                if (mqtt_client_.subscribe("factory/auth/response")) {
                    Serial.println(F("[CloudBridge] Subscribed to 'factory/auth/response'"));
                } else {
                    Serial.println(F("[CloudBridge] ERR: Failed to subscribe to 'factory/auth/response'"));
                }

                publish_network_status(true);
            } else {
                Serial.printf("[CloudBridge] MQTT connect failed, state=%d. Retrying in 3s...\n",
                              mqtt_client_.state());
            }
        }
    }
}

void CloudBridge::broadcast_beacon() {
    if (!esp_now_initialized_ || wifi_channel_ < 1 || wifi_channel_ > 13) {
        return;
    }

    uint32_t now = millis();
    // 500 ms periodic discovery beacon broadcast
    if (now - last_beacon_ms_ >= 500) {
        last_beacon_ms_ = now;

        uint8_t buffer[64];
        int len = build_beacon_packet(wifi_channel_, mac_address_, now, buffer, sizeof(buffer));
        if (len > 0) {
            uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            esp_err_t result = esp_now_send(bcast, buffer, (size_t)len);
            if (result != ESP_OK) {
                // Non-fatal, transient send error
            }
        }
    }
}

void CloudBridge::process_outgoing_auth() {
    if (auth_req_queue_ == NULL) return;

    AuthRequest req = {};
    if (xQueueReceive(auth_req_queue_, &req, 0) == pdTRUE) {
        if (mqtt_connected_) {
            char json_buf[128];
            if (format_auth_request_json(req.user_id, req.pin, json_buf, sizeof(json_buf))) {
                bool published = mqtt_client_.publish("factory/auth/request", json_buf);
                if (published) {
                    Serial.printf("[CloudBridge] Published Auth Request: %s\n", json_buf);
                } else {
                    Serial.println(F("[CloudBridge] ERR: Failed to publish Auth Request to MQTT"));
                }
            }
        } else {
            Serial.println(F("[CloudBridge] WARN: Auth request dropped because MQTT is offline"));
        }
    }
}

void CloudBridge::publish_network_status(bool force) {
    uint32_t now = millis();
    if (!force && (now - last_status_publish_ms_ < 1000)) {
        return;
    }
    last_status_publish_ms_ = now;

    if (net_status_queue_ == NULL) return;

    NetworkStatus status = {};
    status.wifi_connected = wifi_connected_;
    status.mqtt_connected = mqtt_connected_;
    status.wifi_channel = wifi_channel_;
    status.rssi = wifi_connected_ ? (int8_t)WiFi.RSSI() : 0;

    // Send latest status without blocking
    if (xQueueSend(net_status_queue_, &status, 0) != pdTRUE) {
        NetworkStatus dummy = {};
        xQueueReceive(net_status_queue_, &dummy, 0);
        xQueueSend(net_status_queue_, &status, 0);
    }
}

void CloudBridge::mqtt_callback(char* topic, uint8_t* payload, unsigned int length) {
    if (!s_instance || !topic || !payload || length == 0) return;

    Serial.printf("[CloudBridge] MQTT message arrived on topic '%s', len=%u\n", topic, length);

    if (strcmp(topic, "factory/auth/response") == 0) {
        AuthResponse resp = {};
        if (parse_auth_response_json((const char*)payload, (size_t)length, &resp)) {
            Serial.printf("[CloudBridge] Parsed Auth Response: status=%d, user='%s', remaining=%u, lock=%us\n",
                          resp.status, resp.username, resp.remaining_attempts, resp.lockout_seconds);
            if (s_instance->auth_resp_queue_ != NULL) {
                if (xQueueSend(s_instance->auth_resp_queue_, &resp, 0) != pdTRUE) {
                    Serial.println(F("[CloudBridge] WARN: Auth response queue full"));
                }
            }
        } else {
            Serial.println(F("[CloudBridge] ERR: Failed to parse auth response JSON!"));
        }
    }
}

#endif // !UNIT_TEST

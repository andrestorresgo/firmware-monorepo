#ifndef UNIT_TEST

#include "cloud_bridge.h"
#include <Arduino.h>

struct EspNowRxMessage {
    uint8_t mac[6];
    uint8_t data[MAX_PAYLOAD_SIZE + sizeof(FrameHeader)];
    int len;
};

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
      last_status_publish_ms_(0),
      espnow_rx_queue_(NULL),
      actuator_paired_(false) {
    memset(mac_address_, 0, sizeof(mac_address_));
    memset(actuator_mac_, 0, sizeof(actuator_mac_));
    espnow_rx_queue_ = xQueueCreate(8, sizeof(EspNowRxMessage));
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
#ifdef MQTT_INSECURE_TLS
    wifi_client_.setInsecure();
#else
    wifi_client_.setCACert(HIVEMQ_ROOT_CA);
#endif

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
    process_espnow_rx();
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

        // Synchronize system time via SNTP for TLS certificate date validation
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");

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
            esp_now_register_recv_cb(CloudBridge::espnow_recv_callback);

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

                // Subscribe to shape detection topic
                if (mqtt_client_.subscribe("factory/detections")) {
                    Serial.println(F("[CloudBridge] Subscribed to 'factory/detections'"));
                } else {
                    Serial.println(F("[CloudBridge] ERR: Failed to subscribe to 'factory/detections'"));
                }

                // Subscribe to servo actuator topic
                if (mqtt_client_.subscribe("factory/actuator/servo")) {
                    Serial.println(F("[CloudBridge] Subscribed to 'factory/actuator/servo'"));
                } else {
                    Serial.println(F("[CloudBridge] ERR: Failed to subscribe to 'factory/actuator/servo'"));
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
    // 100 ms periodic discovery beacon broadcast for fast Actuator channel scanning (<2s)
    if (now - last_beacon_ms_ >= 100) {
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

void CloudBridge::espnow_recv_callback(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    if (!s_instance || !s_instance->espnow_rx_queue_ || !mac_addr || !data || data_len <= 0) {
        return;
    }
    if ((size_t)data_len > sizeof(EspNowRxMessage::data)) {
        return;
    }
    EspNowRxMessage msg = {};
    memcpy(msg.mac, mac_addr, 6);
    memcpy(msg.data, data, (size_t)data_len);
    msg.len = data_len;
    xQueueSend(s_instance->espnow_rx_queue_, &msg, 0);
}

void CloudBridge::process_espnow_rx() {
    if (!espnow_rx_queue_) return;

    EspNowRxMessage msg = {};
    while (xQueueReceive(espnow_rx_queue_, &msg, 0) == pdTRUE) {
        EspNowPacket pkt = {};
        if (!unpack_packet(msg.data, (size_t)msg.len, &pkt)) {
            Serial.println(F("[CloudBridge] WARN: Received invalid ESP-NOW packet"));
            continue;
        }

        if (pkt.header.opcode == OPCODE_BEACON_ACK) {
            Serial.printf("[CloudBridge] Received BEACON_ACK from Actuator MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          pkt.payload.beacon_ack.actuator_mac[0], pkt.payload.beacon_ack.actuator_mac[1],
                          pkt.payload.beacon_ack.actuator_mac[2], pkt.payload.beacon_ack.actuator_mac[3],
                          pkt.payload.beacon_ack.actuator_mac[4], pkt.payload.beacon_ack.actuator_mac[5]);

            memcpy(actuator_mac_, pkt.payload.beacon_ack.actuator_mac, 6);
            actuator_paired_ = true;

            esp_now_peer_info_t peer = {};
            memcpy(peer.peer_addr, actuator_mac_, 6);
            peer.channel = wifi_channel_;
            peer.encrypt = false;
            peer.ifidx = WIFI_IF_STA;

            if (esp_now_is_peer_exist(actuator_mac_)) {
                esp_now_mod_peer(&peer);
            } else {
                esp_now_add_peer(&peer);
            }
        } else if (pkt.header.opcode == OPCODE_TELEMETRY) {
            Serial.printf("[CloudBridge] Received Telemetry: paused=%d, motor=%d, servo=%d, R=%d, G=%d, B=%d\n",
                          pkt.payload.telemetry.is_paused,
                          pkt.payload.telemetry.motor_state,
                          pkt.payload.telemetry.servo_state,
                          pkt.payload.telemetry.red_count,
                          pkt.payload.telemetry.green_count,
                          pkt.payload.telemetry.blue_count);

            if (mqtt_connected_) {
                char json_buf[160];
                if (format_telemetry_json(&pkt.payload.telemetry, json_buf, sizeof(json_buf))) {
                    if (mqtt_client_.publish("factory/telemetry", json_buf)) {
                        Serial.printf("[CloudBridge] Published telemetry to HiveMQ: %s\n", json_buf);
                    } else {
                        Serial.println(F("[CloudBridge] ERR: Failed to publish telemetry to HiveMQ"));
                    }
                }
            } else {
                Serial.println(F("[CloudBridge] WARN: Dropped telemetry publish, MQTT not connected"));
            }
        } else if (pkt.header.opcode == OPCODE_BATCH_ROLLOVER) {
            Serial.printf("[CloudBridge] Received Batch Rollover: shape=%u, batch_size=%u, time=%lu\n",
                          pkt.payload.batch_rollover.shape_id,
                          pkt.payload.batch_rollover.batch_size,
                          (unsigned long)pkt.payload.batch_rollover.timestamp_ms);

            if (mqtt_connected_) {
                char json_buf[128];
                if (format_rollover_json(&pkt.payload.batch_rollover, json_buf, sizeof(json_buf))) {
                    if (mqtt_client_.publish("factory/rollover", json_buf)) {
                        Serial.printf("[CloudBridge] Published Batch Rollover to HiveMQ: %s\n", json_buf);
                    } else {
                        Serial.println(F("[CloudBridge] ERR: Failed to publish Batch Rollover to HiveMQ"));
                    }
                }
            } else {
                Serial.println(F("[CloudBridge] WARN: Dropped rollover publish, MQTT not connected"));
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
    } else if (strcmp(topic, "factory/detections") == 0) {
        ShapeDetectionPayload det = {};
        if (parse_shape_detection_json((const char*)payload, (size_t)length, &det)) {
            Serial.printf("[CloudBridge] Parsed Shape Detection: shape_id=%u, detection_id=%u\n",
                          det.shape_id, det.detection_id);
            s_instance->forward_shape_detection(&det);
        } else {
            Serial.println(F("[CloudBridge] ERR: Failed to parse shape detection JSON!"));
        }
    } else if (strcmp(topic, "factory/actuator/servo") == 0) {
        uint8_t servo_state = 0;
        if (parse_servo_command_payload((const char*)payload, (size_t)length, &servo_state)) {
            Serial.printf("[CloudBridge] Parsed Servo Command: state=%u (%s)\n",
                          servo_state, (servo_state == SERVO_OPEN) ? "OPEN" : "CLOSED");
            s_instance->forward_servo_command(servo_state);
        } else {
            Serial.println(F("[CloudBridge] ERR: Failed to parse servo command payload!"));
        }
    }
}

void CloudBridge::forward_shape_detection(const struct ShapeDetectionPayload* det) {
    if (!esp_now_initialized_ || !det) return;

    uint8_t buffer[64];
    int len = build_shape_detection_packet(det->shape_id, det->detection_id, buffer, sizeof(buffer));
    if (len <= 0) return;

    const uint8_t* target_mac = actuator_paired_ ? actuator_mac_ : nullptr;
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (!target_mac) {
        target_mac = bcast;
    }

    esp_err_t res = esp_now_send(target_mac, buffer, (size_t)len);
    if (res == ESP_OK) {
        Serial.printf("[CloudBridge] Forwarded Shape Detection (id=%u, shape=%u) via ESP-NOW\n",
                      det->detection_id, det->shape_id);
    } else {
        Serial.printf("[CloudBridge] ERR: esp_now_send shape detection failed, err=%d\n", res);
    }
}

void CloudBridge::forward_servo_command(uint8_t servo_state) {
    if (!esp_now_initialized_) return;

    uint8_t buffer[64];
    int len = build_servo_command_packet(servo_state, buffer, sizeof(buffer));
    if (len <= 0) return;

    const uint8_t* target_mac = actuator_paired_ ? actuator_mac_ : nullptr;
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (!target_mac) {
        target_mac = bcast;
    }

    esp_err_t res = esp_now_send(target_mac, buffer, (size_t)len);
    if (res == ESP_OK) {
        Serial.printf("[CloudBridge] Forwarded Servo Command (state=%u) via ESP-NOW\n", servo_state);
    } else {
        Serial.printf("[CloudBridge] ERR: esp_now_send servo command failed, err=%d\n", res);
    }
}

#endif // !UNIT_TEST

#pragma once

#ifndef UNIT_TEST

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <esp_now.h>
#include "auth_terminal.h"
#include "network_status.h"
#include "auth_protocol.h"
#include "secrets.h"

class CloudBridge {
public:
    CloudBridge(QueueHandle_t auth_req_queue,
                QueueHandle_t auth_resp_queue,
                QueueHandle_t net_status_queue);

    // Initializes Wi-Fi STA mode, TLS certificates, and MQTT client parameters
    void begin();

    // Core 0 periodic processing: Wi-Fi status, MQTT keepalive, beacon broadcast, queue I/O
    void loop();

    bool is_wifi_connected() const;
    bool is_mqtt_connected() const;
    bool is_online() const;
    uint8_t get_wifi_channel() const;

private:
    QueueHandle_t auth_req_queue_;
    QueueHandle_t auth_resp_queue_;
    QueueHandle_t net_status_queue_;

    WiFiClientSecure wifi_client_;
    PubSubClient mqtt_client_;

    bool wifi_connected_;
    bool mqtt_connected_;
    uint8_t wifi_channel_;
    uint8_t mac_address_[6];
    bool esp_now_initialized_;

    uint32_t last_beacon_ms_;
    uint32_t last_mqtt_reconnect_ms_;
    uint32_t last_wifi_check_ms_;
    uint32_t last_status_publish_ms_;

    QueueHandle_t espnow_rx_queue_;
    bool actuator_paired_;
    uint8_t actuator_mac_[6];

    void check_wifi();
    void check_mqtt();
    void broadcast_beacon();
    void process_outgoing_auth();
    void process_espnow_rx();
    void publish_network_status(bool force = false);
    void setup_esp_now();
    void forward_shape_detection(const struct ShapeDetectionPayload* det);
    void forward_servo_command(uint8_t servo_state);
    void forward_motor_command(uint8_t motor_state);

    static void mqtt_callback(char* topic, uint8_t* payload, unsigned int length);
    static void espnow_recv_callback(const uint8_t *mac_addr, const uint8_t *data, int data_len);
    static CloudBridge* s_instance;
};


#endif // !UNIT_TEST

#ifndef UNIT_TEST

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "protocol.h"
#include "link_manager.h"
#include "actuator_state.h"

// ESP-NOW Receive Queue Item
struct EspNowRxMsg {
    uint8_t mac[6];
    uint8_t data[MAX_PAYLOAD_SIZE + sizeof(FrameHeader)];
    int len;
};

// Global State and Drivers
static ActuatorState s_actuator_state;
static LinkManager s_link_manager(120, 3000, 7000, 3); // 120ms dwell, 3s heartbeat, 7s timeout, 3 failures
static uint8_t s_my_mac[6];

// FreeRTOS Queues
static QueueHandle_t s_rx_queue = NULL;
static QueueHandle_t s_event_telemetry_queue = NULL;

// ESP-NOW Callbacks
static void espnow_recv_callback(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    if (!s_rx_queue || !mac_addr || !data || data_len <= 0) return;
    if ((size_t)data_len > sizeof(EspNowRxMsg::data)) return;

    EspNowRxMsg msg = {};
    memcpy(msg.mac, mac_addr, 6);
    memcpy(msg.data, data, (size_t)data_len);
    msg.len = data_len;
    xQueueSend(s_rx_queue, &msg, 0);
}

static void espnow_send_callback(const uint8_t *mac_addr, esp_now_send_status_t status) {
    (void)mac_addr;
    if (status == ESP_NOW_SEND_SUCCESS) {
        s_link_manager.record_send_success();
    } else {
        s_link_manager.record_send_failure();
    }
}

// Network Task (Pinned to Core 0 per ADR-0003)
void networkTask(void* pvParameters) {
    (void)pvParameters;
    Serial.printf("[Core %d] Actuator Network & ESP-NOW task started\n", xPortGetCoreID());

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    WiFi.macAddress(s_my_mac);
    Serial.printf("[Actuator] MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  s_my_mac[0], s_my_mac[1], s_my_mac[2],
                  s_my_mac[3], s_my_mac[4], s_my_mac[5]);

    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println(F("[Actuator] FATAL: Failed to initialize ESP-NOW!"));
        while (1) { delay(1000); }
    }

    esp_now_register_recv_cb(espnow_recv_callback);
    esp_now_register_send_cb(espnow_send_callback);

    s_link_manager.begin(millis());
    Serial.println(F("[Actuator] ESP-NOW initialized, beginning dynamic RF channel sweep (1..13)..."));

    for (;;) {
        uint32_t now = millis();

        // 1. Channel sweeping if in scanning mode
        uint8_t next_ch = 0;
        if (s_link_manager.tick(now, &next_ch)) {
            esp_wifi_set_channel(next_ch, WIFI_SECOND_CHAN_NONE);
            Serial.printf("[Actuator] Scanning channel %d...\n", next_ch);
        }

        // 2. Process incoming ESP-NOW packets from ISR queue
        EspNowRxMsg rx_msg = {};
        while (s_rx_queue && xQueueReceive(s_rx_queue, &rx_msg, 0) == pdTRUE) {
            EspNowPacket rx_pkt = {};
            if (!unpack_packet(rx_msg.data, (size_t)rx_msg.len, &rx_pkt)) {
                Serial.println(F("[Actuator] WARN: Invalid frame received"));
                continue;
            }

            if (rx_pkt.header.opcode == OPCODE_BEACON) {
                const BeaconPayload& b = rx_pkt.payload.beacon;
                Serial.printf("[Actuator] Captured Gateway Discovery Beacon on channel %d from %02X:%02X:%02X:%02X:%02X:%02X\n",
                              b.wifi_channel,
                              b.gateway_mac[0], b.gateway_mac[1], b.gateway_mac[2],
                              b.gateway_mac[3], b.gateway_mac[4], b.gateway_mac[5]);

                if (s_link_manager.handle_beacon(&b, rx_msg.mac, now)) {
                    // Lock radio to discovered channel
                    esp_wifi_set_channel(b.wifi_channel, WIFI_SECOND_CHAN_NONE);

                    // Register Gateway MAC as peer
                    esp_now_peer_info_t gw_peer = {};
                    memcpy(gw_peer.peer_addr, b.gateway_mac, 6);
                    gw_peer.channel = b.wifi_channel;
                    gw_peer.encrypt = false;
                    gw_peer.ifidx = WIFI_IF_STA;

                    if (esp_now_is_peer_exist(gw_peer.peer_addr)) {
                        esp_now_mod_peer(&gw_peer);
                    } else {
                        esp_now_add_peer(&gw_peer);
                    }

                    // Reply with Pairing Acknowledgment
                    EspNowPacket ack_pkt = {};
                    s_link_manager.build_beacon_ack(s_my_mac, &ack_pkt);

                    uint8_t tx_buf[64];
                    int tx_len = pack_packet(&ack_pkt, tx_buf, sizeof(tx_buf));
                    if (tx_len > 0) {
                        esp_now_send(gw_peer.peer_addr, tx_buf, (size_t)tx_len);
                        Serial.println(F("[Actuator] Sent BEACON_ACK to Gateway, link paired!"));
                    }
                }
            } else {
                // Future commands from Gateway (detections, servo) will be processed in Tickets 05 & 06
                s_link_manager.record_activity(now);
            }
        }

        // 3. Check for link inactivity timeout
        if (s_link_manager.check_link_timeout(now)) {
            Serial.println(F("[Actuator] WARN: Gateway link timeout, resuming dynamic channel sweep..."));
        }

        // 4. Send Telemetry Heartbeat (periodic every 3s OR event-driven)
        bool event_pending = (s_event_telemetry_queue && xQueueReceive(s_event_telemetry_queue, &now, 0) == pdTRUE);
        bool periodic_due = s_link_manager.should_send_heartbeat(now);

        if (s_link_manager.is_paired() && (periodic_due || event_pending)) {
            EspNowPacket telem_pkt = {};
            int len = s_actuator_state.build_telemetry_packet(now, &telem_pkt);
            if (len > 0) {
                uint8_t tx_buf[64];
                int packed_len = pack_packet(&telem_pkt, tx_buf, sizeof(tx_buf));
                if (packed_len > 0) {
                    esp_now_send(s_link_manager.get_gateway_mac(), tx_buf, (size_t)packed_len);
                    s_link_manager.record_heartbeat_sent(now);
                    Serial.printf("[Actuator] Emitted Telemetry (%s): pause=%d, motor=%d, servo=%d, R=%d, G=%d, B=%d\n",
                                  event_pending ? "event" : "periodic",
                                  telem_pkt.payload.telemetry.is_paused,
                                  telem_pkt.payload.telemetry.motor_state,
                                  telem_pkt.payload.telemetry.servo_state,
                                  telem_pkt.payload.telemetry.red_count,
                                  telem_pkt.payload.telemetry.green_count,
                                  telem_pkt.payload.telemetry.blue_count);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Actuator & Peripherals Task (Pinned to Core 1 per ADR-0003)
void actuatorTask(void* pvParameters) {
    (void)pvParameters;
    Serial.printf("[Core %d] Actuator Control & Logic task started\n", xPortGetCoreID());

    TickType_t last_wake_time = xTaskGetTickCount();

    for (;;) {
        // Check if state changed, trigger event telemetry
        if (s_actuator_state.is_dirty()) {
            s_actuator_state.clear_dirty();
            if (s_event_telemetry_queue) {
                uint32_t now = millis();
                xQueueSend(s_event_telemetry_queue, &now, 0);
            }
        }

        // 50Hz polling loop (20ms cycle)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(20));
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("========================================"));
    Serial.println(F("[Actuator Board B] Initializing Firmware"));
    Serial.println(F("========================================"));

    // Allocate FreeRTOS Queues
    s_rx_queue = xQueueCreate(8, sizeof(EspNowRxMsg));
    s_event_telemetry_queue = xQueueCreate(4, sizeof(uint32_t));

    if (!s_rx_queue || !s_event_telemetry_queue) {
        Serial.println(F("[FATAL] Failed to create FreeRTOS queues!"));
        while (1) { delay(1000); }
    }

    // Spawn Core 1 Actuator Logic & Peripheral Task
    xTaskCreatePinnedToCore(
        actuatorTask,
        "ActuatorTask",
        4096,
        NULL,
        2,
        NULL,
        1 // Core 1
    );

    // Spawn Core 0 Network & ESP-NOW Task
    xTaskCreatePinnedToCore(
        networkTask,
        "NetworkTask",
        4096,
        NULL,
        1,
        NULL,
        0 // Core 0
    );

    Serial.println(F("[Actuator] Setup complete, FreeRTOS tasks scheduled"));
}

void loop() {
    // Idle background delay
    vTaskDelay(pdMS_TO_TICKS(1000));
}

#endif // !UNIT_TEST
#ifndef UNIT_TEST

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "protocol.h"
#include "link_manager.h"
#include "actuator_state.h"
#include "led_bank_driver.h"
#include "counting_manager.h"
#include "hbridge_motor.h"
#include "servo_gate_driver.h"
#include "button_driver.h"
#include "actuation_manager.h"

// ESP-NOW Receive Queue Item
struct EspNowRxMsg {
    uint8_t mac[6];
    uint8_t data[MAX_PAYLOAD_SIZE + sizeof(FrameHeader)];
    int len;
};

// Global State and Drivers
static ActuatorState s_actuator_state;
static LinkManager s_link_manager(120, 3000, 7000, 3); // 120ms dwell, 3s heartbeat, 7s timeout, 3 failures
static LedBankDriver s_led_driver;
static CountingManager s_counting_manager(s_actuator_state, s_led_driver);
static Esp32HBridgeMotor s_motor;
static Esp32ServoGateDriver s_servo;
static DebouncedButton s_pause_button(50);
static ActuationManager s_actuation_manager(s_actuator_state, s_motor, s_servo, s_pause_button);
static uint8_t s_my_mac[6];

// FreeRTOS Queues
static QueueHandle_t s_rx_queue = NULL;
static QueueHandle_t s_event_telemetry_queue = NULL;
static QueueHandle_t s_detection_queue = NULL;
static QueueHandle_t s_servo_queue = NULL;
static QueueHandle_t s_tx_rollover_queue = NULL;

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
            } else if (rx_pkt.header.opcode == OPCODE_SHAPE_DETECTION) {
                s_link_manager.record_activity(now);
                Serial.printf("[Actuator] Received SHAPE_DETECTION from Gateway: shape=%u, id=%u\n",
                              rx_pkt.payload.shape_detection.shape_id,
                              rx_pkt.payload.shape_detection.detection_id);
                if (s_detection_queue) {
                    if (xQueueSend(s_detection_queue, &rx_pkt.payload.shape_detection, 0) != pdTRUE) {
                        Serial.println(F("[Actuator] WARN: Detection queue full, dropping packet"));
                    }
                }
            } else if (rx_pkt.header.opcode == OPCODE_SERVO_COMMAND) {
                s_link_manager.record_activity(now);
                Serial.printf("[Actuator] Received SERVO_COMMAND from Gateway: state=%u\n",
                              rx_pkt.payload.servo_command.servo_state);
                if (s_servo_queue) {
                    if (xQueueSend(s_servo_queue, &rx_pkt.payload.servo_command, 0) != pdTRUE) {
                        Serial.println(F("[Actuator] WARN: Servo queue full, dropping command"));
                    }
                }
            } else {
                s_link_manager.record_activity(now);
            }
        }

        // 3. Check for link inactivity timeout
        if (s_link_manager.check_link_timeout(now)) {
            Serial.println(F("[Actuator] WARN: Gateway link timeout, resuming dynamic channel sweep..."));
        }

        // 4. Send Batch Rollover notifications to Gateway
        BatchRolloverPayload rollover_msg = {};
        while (s_tx_rollover_queue && xQueueReceive(s_tx_rollover_queue, &rollover_msg, 0) == pdTRUE) {
            if (s_link_manager.is_paired()) {
                EspNowPacket rollover_pkt = {};
                rollover_pkt.header.magic = ESPNOW_MAGIC_BYTE;
                rollover_pkt.header.opcode = OPCODE_BATCH_ROLLOVER;
                rollover_pkt.header.payload_len = sizeof(BatchRolloverPayload);
                rollover_pkt.payload.batch_rollover = rollover_msg;

                uint8_t tx_buf[64];
                int packed_len = pack_packet(&rollover_pkt, tx_buf, sizeof(tx_buf));
                if (packed_len > 0) {
                    esp_now_send(s_link_manager.get_gateway_mac(), tx_buf, (size_t)packed_len);
                    Serial.printf("[Actuator] Emitted Batch Rollover to Gateway: shape=%u, size=%u, time=%lu\n",
                                  rollover_msg.shape_id, rollover_msg.batch_size, (unsigned long)rollover_msg.timestamp_ms);
                }
            }
        }

        // 5. Send Telemetry Heartbeat (periodic every 3s OR event-driven)
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

    s_actuation_manager.begin();
    s_counting_manager.begin();
    TickType_t last_wake_time = xTaskGetTickCount();

    for (;;) {
        uint32_t now = millis();

        // 1. Process momentary push button on GPIO 25 (Active-LOW with INPUT_PULLUP)
        bool raw_pressed = (digitalRead(PIN_PAUSE_BUTTON) == LOW);
        if (s_actuation_manager.handle_pause_button(now, raw_pressed)) {
            Serial.printf("[Actuator] Machine Pause TOGGLED -> is_paused=%d, motor_running=%d, duty=%u%%\n",
                          s_actuation_manager.is_paused(),
                          s_actuation_manager.is_motor_running(),
                          s_actuation_manager.get_motor_duty());
        }

        // 2. Process incoming servo commands from Core 0
        ServoCommandPayload servo_cmd = {};
        while (s_servo_queue && xQueueReceive(s_servo_queue, &servo_cmd, 0) == pdTRUE) {
            bool state_changed = s_actuation_manager.handle_servo_command(servo_cmd.servo_state);
            Serial.printf("[Actuator] Handled servo command (%u): changed=%d, angle=%d, is_paused=%d\n",
                          servo_cmd.servo_state, state_changed,
                          s_actuation_manager.get_servo_angle(),
                          s_actuation_manager.is_paused());
        }

        // 3. Process incoming shape detection commands from Core 0
        ShapeDetectionPayload det = {};
        while (s_detection_queue && xQueueReceive(s_detection_queue, &det, 0) == pdTRUE) {
            bool accepted = s_counting_manager.handle_shape_detection(det.shape_id, now);
            Serial.printf("[Actuator] Handled shape %u (id=%u): accepted=%d, count=%u, in_delay=%d\n",
                          det.shape_id, det.detection_id, accepted,
                          s_counting_manager.get_count(det.shape_id),
                          s_counting_manager.is_in_observation_delay(det.shape_id));
        }

        // 4. Advance 800ms Observation Delay timers and check for Batch Rollovers
        BatchRolloverPayload rollover = {};
        while (s_counting_manager.tick(now, &rollover)) {
            Serial.printf("[Actuator] Observation delay expired! Batch rollover for shape %u, count reset to 0\n",
                          rollover.shape_id);
            if (s_tx_rollover_queue) {
                if (xQueueSend(s_tx_rollover_queue, &rollover, 0) != pdTRUE) {
                    Serial.println(F("[Actuator] WARN: Rollover queue full"));
                }
            }
        }

        // 5. Check if state changed, trigger event telemetry
        if (s_actuator_state.is_dirty()) {
            s_actuator_state.clear_dirty();
            if (s_event_telemetry_queue) {
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

    // Configure push button GPIO with internal pullup
    pinMode(PIN_PAUSE_BUTTON, INPUT_PULLUP);

    // Allocate FreeRTOS Queues
    s_rx_queue = xQueueCreate(8, sizeof(EspNowRxMsg));
    s_event_telemetry_queue = xQueueCreate(4, sizeof(uint32_t));
    s_detection_queue = xQueueCreate(8, sizeof(ShapeDetectionPayload));
    s_servo_queue = xQueueCreate(4, sizeof(ServoCommandPayload));
    s_tx_rollover_queue = xQueueCreate(4, sizeof(BatchRolloverPayload));

    if (!s_rx_queue || !s_event_telemetry_queue || !s_detection_queue || !s_servo_queue || !s_tx_rollover_queue) {
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
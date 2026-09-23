#ifndef UNIT_TEST

#include <Arduino.h>
#include <Keypad.h>
#include <ArduinoJson.h>
#include "protocol.h"
#include "auth_terminal.h"
#include "terminal_display.h"
#include "network_status.h"
#include "cloud_bridge.h"

// 4x4 Matrix Keypad Configuration
// Spec: Rows GPIO 13, 12, 14, 27; Cols GPIO 26, 25, 33, 32
static const byte KEYPAD_ROWS = 4;
static const byte KEYPAD_COLS = 4;
static char KEYPAD_KEYS[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};
static byte KEYPAD_ROW_PINS[KEYPAD_ROWS] = {13, 12, 14, 27};
static byte KEYPAD_COL_PINS[KEYPAD_COLS] = {26, 25, 33, 32};

static Keypad s_keypad = Keypad(makeKeymap(KEYPAD_KEYS), KEYPAD_ROW_PINS, KEYPAD_COL_PINS, KEYPAD_ROWS, KEYPAD_COLS);

// Terminal State & Display Driver
static AuthTerminal s_terminal;
static TerminalDisplay s_display;

// FreeRTOS Inter-Core Communication Queues
QueueHandle_t g_auth_request_queue = NULL;
QueueHandle_t g_auth_response_queue = NULL;
QueueHandle_t g_network_status_queue = NULL;

// Terminal UI & Keypad Task (Pinned to Core 1 per ADR-0003)
void terminalTask(void* pvParameters) {
    (void)pvParameters;
    Serial.printf("[Core %d] Terminal & Keypad task started\n", xPortGetCoreID());

    s_display.begin();
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t last_tick_ms = millis();
    TerminalState last_state = s_terminal.get_state();
    bool state_changed = true;

    for (;;) {
        uint32_t now = millis();
        uint32_t delta_ms = now - last_tick_ms;
        last_tick_ms = now;

        // 1. Check for Network Status updates from Core 0
        if (g_network_status_queue != NULL) {
            NetworkStatus status = {};
            if (xQueueReceive(g_network_status_queue, &status, 0) == pdTRUE) {
                bool online = is_system_online(&status);
                if (online != s_terminal.is_network_connected()) {
                    s_terminal.set_network_connected(online);
                    state_changed = true;
                    Serial.printf("[Terminal] Network status updated: online=%d (WiFi=%d, MQTT=%d, Ch=%d)\n",
                                  online, status.wifi_connected, status.mqtt_connected, status.wifi_channel);
                }
            }
        }

        // 2. Scan Keypad
        char key = s_keypad.getKey();
        if (key != NO_KEY) {
            Serial.printf("[Keypad] Key pressed: '%c'\n", key);
            bool submitted = s_terminal.handle_key(key);
            state_changed = true;

            if (submitted) {
                AuthRequest req = {};
                if (s_terminal.get_auth_request(&req)) {
                    char json_payload[128];
                    s_terminal.format_auth_request_json(json_payload, sizeof(json_payload));
                    Serial.printf("[Terminal] Submitting Auth Request JSON: %s\n", json_payload);

                    if (g_auth_request_queue != NULL) {
                        if (xQueueSend(g_auth_request_queue, &req, 0) != pdTRUE) {
                            Serial.println(F("[Terminal] WARN: Auth request queue full"));
                        }
                    }
                }
            }
        }

        // 3. Check for incoming server responses from Core 0
        if (g_auth_response_queue != NULL) {
            AuthResponse resp = {};
            if (xQueueReceive(g_auth_response_queue, &resp, 0) == pdTRUE) {
                Serial.printf("[Terminal] Received Auth Response: status=%d, user='%s', remaining=%d, lock=%ds\n",
                              resp.status, resp.username, resp.remaining_attempts, resp.lockout_seconds);
                s_terminal.handle_auth_response(resp);
                state_changed = true;
            }
        }

        // 4. Advance timers and state machines
        s_terminal.tick(delta_ms);

        // 5. Update display if state changed or in active timer states
        TerminalState current_state = s_terminal.get_state();
        if (current_state != last_state || state_changed ||
            current_state == STATE_AUTH_LOCKED ||
            current_state == STATE_AUTH_SUCCESS ||
            current_state == STATE_ENTER_USER_ID ||
            current_state == STATE_ENTER_PIN) {
            s_display.render(s_terminal);
            last_state = current_state;
            state_changed = false;
        }

        // 50Hz polling rate (20ms debounce and scan period)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(20));
    }
}

// Network Bridge Task (Pinned to Core 0 per ADR-0003)
void networkTask(void* pvParameters) {
    (void)pvParameters;
    Serial.printf("[Core %d] Network bridge task started\n", xPortGetCoreID());

    CloudBridge bridge(g_auth_request_queue, g_auth_response_queue, g_network_status_queue);
    bridge.begin();

    for (;;) {
        bridge.loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println(F("========================================"));
    Serial.println(F("[Gateway Board A] Initializing Firmware"));
    Serial.println(F("========================================"));

    // Create FreeRTOS inter-core communication queues
    g_auth_request_queue = xQueueCreate(4, sizeof(AuthRequest));
    g_auth_response_queue = xQueueCreate(4, sizeof(AuthResponse));
    g_network_status_queue = xQueueCreate(4, sizeof(NetworkStatus));

    if (!g_auth_request_queue || !g_auth_response_queue || !g_network_status_queue) {
        Serial.println(F("[FATAL] Failed to create FreeRTOS queues!"));
        while (1) { delay(1000); }
    }

    // Spawn Terminal Task on Core 1 (UI, Keypad, OLED)
    xTaskCreatePinnedToCore(
        terminalTask,
        "TerminalTask",
        4096,
        NULL,
        2,
        NULL,
        1 // Core 1
    );

    // Spawn Network Task on Core 0 (MQTT, Wi-Fi, ESP-NOW) with 8192 stack size for TLS
    xTaskCreatePinnedToCore(
        networkTask,
        "NetworkTask",
        8192,
        NULL,
        1,
        NULL,
        0 // Core 0
    );

    Serial.println(F("[Gateway] Setup complete, FreeRTOS tasks scheduled"));
}

void loop() {
    // Arduino loop task idle / monitoring
    vTaskDelay(pdMS_TO_TICKS(1000));
}

#endif // !UNIT_TEST
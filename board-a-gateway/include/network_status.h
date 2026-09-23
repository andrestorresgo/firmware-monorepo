#pragma once

#include <stdint.h>
#include <stdbool.h>

struct NetworkStatus {
    bool wifi_connected;
    bool mqtt_connected;
    uint8_t wifi_channel;
    int8_t rssi;
};

// System is considered fully online only when both Wi-Fi and Cloud MQTT are connected
static inline bool is_system_online(const struct NetworkStatus* status) {
    if (!status) return false;
    return status->wifi_connected && status->mqtt_connected;
}

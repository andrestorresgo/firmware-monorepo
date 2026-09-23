#include "auth_protocol.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

bool parse_auth_response_json(const char* json_str, size_t len, struct AuthResponse* out_resp) {
    if (!json_str || len == 0 || !out_resp) {
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json_str, len);
    if (err) {
        return false;
    }

    const char* status_str = doc["status"];
    if (!status_str) {
        return false;
    }

    memset(out_resp, 0, sizeof(struct AuthResponse));

    if (strcmp(status_str, "AUTH_OK") == 0 || strcmp(status_str, "OK") == 0) {
        out_resp->status = AUTH_STATUS_OK;
        const char* username = doc["username"];
        if (username) {
            strncpy(out_resp->username, username, sizeof(out_resp->username) - 1);
            out_resp->username[sizeof(out_resp->username) - 1] = '\0';
        }
        out_resp->remaining_attempts = doc["remaining_attempts"] | 0;
        out_resp->lockout_seconds = doc["lockout_seconds"] | 0;
        return true;
    } else if (strcmp(status_str, "INVALID_PIN") == 0) {
        out_resp->status = AUTH_STATUS_INVALID_PIN;
        out_resp->remaining_attempts = doc["remaining_attempts"] | 0;
        out_resp->lockout_seconds = doc["lockout_seconds"] | 0;
        return true;
    } else if (strcmp(status_str, "USER_LOCKED") == 0) {
        out_resp->status = AUTH_STATUS_USER_LOCKED;
        out_resp->remaining_attempts = doc["remaining_attempts"] | 0;
        out_resp->lockout_seconds = doc["lockout_seconds"] | 0;
        return true;
    } else if (strcmp(status_str, "USER_NOT_FOUND") == 0) {
        out_resp->status = AUTH_STATUS_USER_NOT_FOUND;
        return true;
    }

    return false;
}

bool format_auth_request_json(uint32_t user_id, const char* pin, char* out_buf, size_t max_len) {
    if (!out_buf || max_len == 0 || !pin) {
        return false;
    }

    int written = snprintf(out_buf, max_len, "{\"user_id\": %u, \"pin\": \"%s\"}", user_id, pin);
    return (written > 0 && (size_t)written < max_len);
}

int build_beacon_packet(uint8_t wifi_channel, const uint8_t* gateway_mac, uint32_t uptime_ms, uint8_t* out_buf, size_t max_len) {
    if (wifi_channel < 1 || wifi_channel > 13) {
        return -1;
    }
    if (!gateway_mac || !out_buf) {
        return -1;
    }
    size_t required_len = sizeof(struct FrameHeader) + sizeof(struct BeaconPayload);
    if (max_len < required_len) {
        return -1;
    }

    struct EspNowPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.header.magic = ESPNOW_MAGIC_BYTE;
    pkt.header.opcode = OPCODE_BEACON;
    pkt.header.payload_len = sizeof(struct BeaconPayload);
    pkt.payload.beacon.wifi_channel = wifi_channel;
    memcpy(pkt.payload.beacon.gateway_mac, gateway_mac, 6);
    pkt.payload.beacon.uptime_ms = uptime_ms;

    return pack_packet(&pkt, out_buf, max_len);
}

bool format_telemetry_json(const struct TelemetryPayload* telemetry, char* out_buf, size_t max_len) {
    if (!telemetry || !out_buf || max_len == 0) {
        return false;
    }

    const char* is_paused_str = telemetry->is_paused ? "true" : "false";
    const char* motor_state_str = telemetry->motor_state ? "true" : "false";
    const char* servo_state_str = (telemetry->servo_state != 0) ? "true" : "false";

    int written = snprintf(out_buf, max_len,
                           "{\"is_paused\":%s,\"motor_state\":%s,\"servo_state\":%s,\"red_count\":%u,\"green_count\":%u,\"blue_count\":%u}",
                           is_paused_str, motor_state_str, servo_state_str,
                           telemetry->red_count, telemetry->green_count, telemetry->blue_count);

    return (written > 0 && (size_t)written < max_len);
}


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

bool parse_shape_detection_json(const char* json_str, size_t len, struct ShapeDetectionPayload* out_payload) {
    if (!json_str || len == 0 || !out_payload) {
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json_str, len);
    if (err) {
        return false;
    }

    uint8_t shape_id = doc["shape_id"] | 0;
    if (shape_id == 0) {
        const char* name = doc["shape_name"];
        if (name) {
            if (strcasecmp(name, "circle") == 0 || strcasecmp(name, "red") == 0) {
                shape_id = SHAPE_CIRCLE;
            } else if (strcasecmp(name, "triangle") == 0 || strcasecmp(name, "green") == 0) {
                shape_id = SHAPE_TRIANGLE;
            } else if (strcasecmp(name, "square") == 0 || strcasecmp(name, "blue") == 0) {
                shape_id = SHAPE_SQUARE;
            }
        }
    }

    if (shape_id < SHAPE_CIRCLE || shape_id > SHAPE_SQUARE) {
        return false;
    }

    out_payload->shape_id = shape_id;
    out_payload->detection_id = doc["detection_id"] | 0;
    return true;
}

int build_shape_detection_packet(uint8_t shape_id, uint32_t detection_id, uint8_t* out_buf, size_t max_len) {
    if (shape_id < SHAPE_CIRCLE || shape_id > SHAPE_SQUARE) {
        return -1;
    }
    if (!out_buf) {
        return -1;
    }
    size_t required_len = sizeof(struct FrameHeader) + sizeof(struct ShapeDetectionPayload);
    if (max_len < required_len) {
        return -1;
    }

    struct EspNowPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.header.magic = ESPNOW_MAGIC_BYTE;
    pkt.header.opcode = OPCODE_SHAPE_DETECTION;
    pkt.header.payload_len = sizeof(struct ShapeDetectionPayload);
    pkt.payload.shape_detection.shape_id = shape_id;
    pkt.payload.shape_detection.detection_id = detection_id;

    return pack_packet(&pkt, out_buf, max_len);
}

bool format_rollover_json(const struct BatchRolloverPayload* rollover, char* out_buf, size_t max_len) {
    if (!rollover || !out_buf || max_len == 0) {
        return false;
    }

    const char* shape_name = "unknown";
    switch (rollover->shape_id) {
        case SHAPE_CIRCLE:
            shape_name = "circle";
            break;
        case SHAPE_TRIANGLE:
            shape_name = "triangle";
            break;
        case SHAPE_SQUARE:
            shape_name = "square";
            break;
        default:
            break;
    }

    int written = snprintf(out_buf, max_len,
                           "{\"shape_id\":%u,\"shape_name\":\"%s\",\"timestamp\":%lu}",
                           rollover->shape_id, shape_name, (unsigned long)rollover->timestamp_ms);

    return (written > 0 && (size_t)written < max_len);
}

bool parse_servo_command_payload(const char* payload, size_t len, uint8_t* out_servo_state) {
    if (!payload || len == 0 || !out_servo_state) {
        return false;
    }

    // Trim leading whitespace
    while (len > 0 && (*payload == ' ' || *payload == '\t' || *payload == '\r' || *payload == '\n' || *payload == '\"')) {
        payload++;
        len--;
    }
    // Trim trailing whitespace
    while (len > 0 && (payload[len - 1] == ' ' || payload[len - 1] == '\t' || payload[len - 1] == '\r' || payload[len - 1] == '\n' || payload[len - 1] == '\"')) {
        len--;
    }
    if (len == 0) {
        return false;
    }

    // If starts with '{', attempt JSON parsing
    if (*payload == '{') {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload, len);
        if (!err) {
            // Check "state", "position", "command"
            const char* str_val = doc["state"] | doc["position"] | doc["command"] | (const char*)nullptr;
            if (str_val) {
                if (strcasecmp(str_val, "OPEN") == 0 || strcmp(str_val, "1") == 0 || strcasecmp(str_val, "true") == 0) {
                    *out_servo_state = SERVO_OPEN;
                    return true;
                } else if (strcasecmp(str_val, "CLOSED") == 0 || strcasecmp(str_val, "CLOSE") == 0 || strcmp(str_val, "0") == 0 || strcasecmp(str_val, "false") == 0) {
                    *out_servo_state = SERVO_CLOSED;
                    return true;
                }
            }

            // Check numeric "servo_state", "state", "position"
            if (doc["servo_state"].is<int>()) {
                int val = doc["servo_state"].as<int>();
                if (val == 0 || val == 1) {
                    *out_servo_state = (uint8_t)val;
                    return true;
                }
            }
            if (doc["state"].is<int>()) {
                int val = doc["state"].as<int>();
                if (val == 0 || val == 1) {
                    *out_servo_state = (uint8_t)val;
                    return true;
                }
            }
            if (doc["position"].is<int>()) {
                int val = doc["position"].as<int>();
                if (val == 0 || val == 1) {
                    *out_servo_state = (uint8_t)val;
                    return true;
                }
            }
        }
    }

    // Fallback: check raw string payload (e.g. "OPEN", "CLOSED", "1", "0")
    if ((len == 4 && strncasecmp(payload, "OPEN", 4) == 0) ||
        (len == 1 && *payload == '1') ||
        (len == 4 && strncasecmp(payload, "true", 4) == 0)) {
        *out_servo_state = SERVO_OPEN;
        return true;
    } else if ((len == 6 && strncasecmp(payload, "CLOSED", 6) == 0) ||
               (len == 5 && strncasecmp(payload, "CLOSE", 5) == 0) ||
               (len == 1 && *payload == '0') ||
               (len == 5 && strncasecmp(payload, "false", 5) == 0)) {
        *out_servo_state = SERVO_CLOSED;
        return true;
    }

    return false;
}

int build_servo_command_packet(uint8_t servo_state, uint8_t* out_buf, size_t max_len) {
    if (servo_state != SERVO_CLOSED && servo_state != SERVO_OPEN) {
        return -1;
    }
    if (!out_buf) {
        return -1;
    }
    size_t required_len = sizeof(struct FrameHeader) + sizeof(struct ServoCommandPayload);
    if (max_len < required_len) {
        return -1;
    }

    struct EspNowPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.header.magic = ESPNOW_MAGIC_BYTE;
    pkt.header.opcode = OPCODE_SERVO_COMMAND;
    pkt.header.payload_len = sizeof(struct ServoCommandPayload);
    pkt.payload.servo_command.servo_state = servo_state;

    return pack_packet(&pkt, out_buf, max_len);
}




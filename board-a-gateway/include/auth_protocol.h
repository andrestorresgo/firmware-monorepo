#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "auth_terminal.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// Parses JSON string payload received on factory/auth/response into AuthResponse struct
// Returns true if parsing was successful and status was recognized
bool parse_auth_response_json(const char* json_str, size_t len, struct AuthResponse* out_resp);

// Formats JSON request string matching {"user_id": <int>, "pin": "<str>"} for publishing to factory/auth/request
// Returns true if output buffer was sufficiently large and formatting succeeded
bool format_auth_request_json(uint32_t user_id, const char* pin, char* out_buf, size_t max_len);

// Constructs and packs a validated ESP-NOW beacon packet (OPCODE_BEACON) ready for transmission
// Returns the total packed frame length in bytes on success, or negative on validation error
int build_beacon_packet(uint8_t wifi_channel, const uint8_t* gateway_mac, uint32_t uptime_ms, uint8_t* out_buf, size_t max_len);

// Formats JSON string matching {"is_paused": <bool>, "motor_state": <bool>, "servo_state": <bool>, "red_count": <int>, "green_count": <int>, "blue_count": <int>}
// for publishing to factory/telemetry
// Returns true if output buffer was sufficiently large and formatting succeeded
bool format_telemetry_json(const struct TelemetryPayload* telemetry, char* out_buf, size_t max_len);

// Parses JSON string payload received on factory/detections into ShapeDetectionPayload struct
// Supports {"shape_id": <int>, "shape_name": "<str>"}
// Returns true if parsing was successful and shape_id is valid (1, 2, or 3)
bool parse_shape_detection_json(const char* json_str, size_t len, struct ShapeDetectionPayload* out_payload);

// Constructs and packs a validated ESP-NOW shape detection packet (OPCODE_SHAPE_DETECTION) ready for transmission
// Returns the total packed frame length in bytes on success, or negative on validation error
int build_shape_detection_packet(uint8_t shape_id, uint32_t detection_id, uint8_t* out_buf, size_t max_len);

// Formats JSON string matching {"shape_id": <int>, "shape_name": "<str>", "timestamp": <int>}
// for publishing to factory/rollover
// Returns true if output buffer was sufficiently large and formatting succeeded
bool format_rollover_json(const struct BatchRolloverPayload* rollover, char* out_buf, size_t max_len);

// Parses string or JSON payload received on factory/actuator/servo into servo state (SERVO_CLOSED=0, SERVO_OPEN=1)
// Supports "OPEN", "CLOSED", "open", "closed", "1", "0", and JSON {"state": "OPEN"}, {"servo_state": 1}, etc.
// Returns true if parsing was successful and state was valid
bool parse_servo_command_payload(const char* payload, size_t len, uint8_t* out_servo_state);

// Constructs and packs a validated ESP-NOW servo command packet (OPCODE_SERVO_COMMAND) ready for transmission
// Returns the total packed frame length in bytes on success, or negative on validation error
int build_servo_command_packet(uint8_t servo_state, uint8_t* out_buf, size_t max_len);

#ifdef __cplusplus
}
#endif


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

#ifdef __cplusplus
}
#endif


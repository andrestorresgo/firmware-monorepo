#include <unity.h>
#include <string.h>
#include "auth_protocol.h"
#include "network_status.h"

void setUp(void) {}
void tearDown(void) {}

void test_parse_auth_response_ok(void) {
    const char* json = "{\"status\":\"AUTH_OK\",\"username\":\"Alice\",\"remaining_attempts\":2,\"lockout_seconds\":0}";
    AuthResponse resp = {};
    
    bool ok = parse_auth_response_json(json, strlen(json), &resp);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(AUTH_STATUS_OK, resp.status);
    TEST_ASSERT_EQUAL_STRING("Alice", resp.username);
    TEST_ASSERT_EQUAL_UINT8(2, resp.remaining_attempts);
    TEST_ASSERT_EQUAL_UINT32(0, resp.lockout_seconds);
}

void test_parse_auth_response_invalid_pin(void) {
    const char* json = "{\"status\":\"INVALID_PIN\",\"remaining_attempts\":1,\"lockout_seconds\":0}";
    AuthResponse resp = {};
    
    bool ok = parse_auth_response_json(json, strlen(json), &resp);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(AUTH_STATUS_INVALID_PIN, resp.status);
    TEST_ASSERT_EQUAL_UINT8(1, resp.remaining_attempts);
    TEST_ASSERT_EQUAL_UINT32(0, resp.lockout_seconds);
}

void test_parse_auth_response_user_locked(void) {
    const char* json = "{\"status\":\"USER_LOCKED\",\"remaining_attempts\":0,\"lockout_seconds\":60}";
    AuthResponse resp = {};
    
    bool ok = parse_auth_response_json(json, strlen(json), &resp);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(AUTH_STATUS_USER_LOCKED, resp.status);
    TEST_ASSERT_EQUAL_UINT32(60, resp.lockout_seconds);
}

void test_parse_auth_response_user_not_found(void) {
    const char* json = "{\"status\":\"USER_NOT_FOUND\"}";
    AuthResponse resp = {};
    
    bool ok = parse_auth_response_json(json, strlen(json), &resp);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(AUTH_STATUS_USER_NOT_FOUND, resp.status);
}

void test_parse_auth_response_malformed_and_edge_cases(void) {
    AuthResponse resp = {};
    TEST_ASSERT_FALSE(parse_auth_response_json(NULL, 0, &resp));
    TEST_ASSERT_FALSE(parse_auth_response_json("{}", 2, NULL));
    
    // Broken JSON syntax
    const char* broken = "{status:AUTH_OK";
    TEST_ASSERT_FALSE(parse_auth_response_json(broken, strlen(broken), &resp));
    
    // Missing status field
    const char* no_status = "{\"username\":\"Bob\"}";
    TEST_ASSERT_FALSE(parse_auth_response_json(no_status, strlen(no_status), &resp));
    
    // Unknown status string
    const char* unknown_status = "{\"status\":\"UNKNOWN_CODE\"}";
    TEST_ASSERT_FALSE(parse_auth_response_json(unknown_status, strlen(unknown_status), &resp));
}

void test_format_auth_request_json_valid(void) {
    char buf[64];
    bool ok = format_auth_request_json(42, "1234", buf, sizeof(buf));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("{\"user_id\": 42, \"pin\": \"1234\"}", buf);
}

void test_format_auth_request_json_buffer_too_small(void) {
    char buf[10];
    bool ok = format_auth_request_json(12345, "9999", buf, sizeof(buf));
    TEST_ASSERT_FALSE(ok);
}

void test_build_beacon_packet_success(void) {
    uint8_t mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    uint8_t buf[64];
    
    int len = build_beacon_packet(6, mac, 54321, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL(sizeof(FrameHeader) + sizeof(BeaconPayload), (size_t)len);
    
    // Frame must pass standard protocol validation
    TEST_ASSERT_TRUE(validate_frame(buf, (size_t)len));
    
    // Unpack and verify contents
    EspNowPacket pkt = {};
    TEST_ASSERT_TRUE(unpack_packet(buf, (size_t)len, &pkt));
    TEST_ASSERT_EQUAL(ESPNOW_MAGIC_BYTE, pkt.header.magic);
    TEST_ASSERT_EQUAL(OPCODE_BEACON, pkt.header.opcode);
    TEST_ASSERT_EQUAL(sizeof(BeaconPayload), pkt.header.payload_len);
    TEST_ASSERT_EQUAL_UINT8(6, pkt.payload.beacon.wifi_channel);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(mac, pkt.payload.beacon.gateway_mac, 6);
    TEST_ASSERT_EQUAL_UINT32(54321, pkt.payload.beacon.uptime_ms);
}

void test_build_beacon_packet_validation_errors(void) {
    uint8_t mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t buf[64];
    
    // Invalid RF channels (Wi-Fi 2.4 GHz allows 1 to 13, channel 0 or >13 are invalid)
    TEST_ASSERT_EQUAL(-1, build_beacon_packet(0, mac, 100, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(-1, build_beacon_packet(14, mac, 100, buf, sizeof(buf)));
    
    // NULL MAC address
    TEST_ASSERT_EQUAL(-1, build_beacon_packet(1, NULL, 100, buf, sizeof(buf)));
    
    // Buffer too small for frame header + beacon payload (15 bytes)
    TEST_ASSERT_EQUAL(-1, build_beacon_packet(1, mac, 100, buf, 14));
}

void test_network_status_helper(void) {
    NetworkStatus status = {};
    TEST_ASSERT_FALSE(is_system_online(NULL));
    
    status.wifi_connected = false;
    status.mqtt_connected = false;
    TEST_ASSERT_FALSE(is_system_online(&status));
    
    status.wifi_connected = true;
    status.mqtt_connected = false;
    TEST_ASSERT_FALSE(is_system_online(&status));
    
    status.wifi_connected = false;
    status.mqtt_connected = true;
    TEST_ASSERT_FALSE(is_system_online(&status));
    
    status.wifi_connected = true;
    status.mqtt_connected = true;
    TEST_ASSERT_TRUE(is_system_online(&status));
}

void test_format_telemetry_json_valid(void) {
    TelemetryPayload payload = {};
    payload.is_paused = 0;
    payload.motor_state = 1;
    payload.servo_state = SERVO_OPEN; // 1
    payload.red_count = 2;
    payload.green_count = 5;
    payload.blue_count = 0;
    payload.uptime_ms = 12345;

    char buf[128];
    bool ok = format_telemetry_json(&payload, buf, sizeof(buf));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("{\"is_paused\":false,\"motor_state\":true,\"servo_state\":true,\"red_count\":2,\"green_count\":5,\"blue_count\":0}", buf);

    // Another case with paused = true, motor = false, servo = closed
    payload.is_paused = 1;
    payload.motor_state = 0;
    payload.servo_state = SERVO_CLOSED; // 0
    payload.red_count = 4;
    payload.green_count = 1;
    payload.blue_count = 3;

    ok = format_telemetry_json(&payload, buf, sizeof(buf));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("{\"is_paused\":true,\"motor_state\":false,\"servo_state\":false,\"red_count\":4,\"green_count\":1,\"blue_count\":3}", buf);
}

void test_format_telemetry_json_boundary_and_null(void) {
    TelemetryPayload payload = {};
    char buf[128];

    // NULL pointer handling
    TEST_ASSERT_FALSE(format_telemetry_json(NULL, buf, sizeof(buf)));
    TEST_ASSERT_FALSE(format_telemetry_json(&payload, NULL, sizeof(buf)));
    TEST_ASSERT_FALSE(format_telemetry_json(&payload, buf, 0));

    // Buffer too small
    char small_buf[30];
    TEST_ASSERT_FALSE(format_telemetry_json(&payload, small_buf, sizeof(small_buf)));
}

void test_parse_shape_detection_json(void) {
    ShapeDetectionPayload payload = {};

    // By shape_id and name
    const char* json1 = "{\"shape_id\":1,\"shape_name\":\"circle\",\"detection_id\":101}";
    TEST_ASSERT_TRUE(parse_shape_detection_json(json1, strlen(json1), &payload));
    TEST_ASSERT_EQUAL_UINT8(SHAPE_CIRCLE, payload.shape_id);
    TEST_ASSERT_EQUAL_UINT32(101, payload.detection_id);

    // By shape_name only (triangle)
    const char* json2 = "{\"shape_name\":\"triangle\",\"detection_id\":202}";
    TEST_ASSERT_TRUE(parse_shape_detection_json(json2, strlen(json2), &payload));
    TEST_ASSERT_EQUAL_UINT8(SHAPE_TRIANGLE, payload.shape_id);
    TEST_ASSERT_EQUAL_UINT32(202, payload.detection_id);

    // By shape_name only (square)
    const char* json3 = "{\"shape_name\":\"square\"}";
    TEST_ASSERT_TRUE(parse_shape_detection_json(json3, strlen(json3), &payload));
    TEST_ASSERT_EQUAL_UINT8(SHAPE_SQUARE, payload.shape_id);
    TEST_ASSERT_EQUAL_UINT32(0, payload.detection_id);

    // Invalid shape ID
    const char* json_invalid = "{\"shape_id\":4}";
    TEST_ASSERT_FALSE(parse_shape_detection_json(json_invalid, strlen(json_invalid), &payload));

    // Unknown shape name
    const char* json_unknown = "{\"shape_name\":\"hexagon\"}";
    TEST_ASSERT_FALSE(parse_shape_detection_json(json_unknown, strlen(json_unknown), &payload));

    // Malformed JSON / NULL
    TEST_ASSERT_FALSE(parse_shape_detection_json("{bad_json", 9, &payload));
    TEST_ASSERT_FALSE(parse_shape_detection_json(NULL, 10, &payload));
    TEST_ASSERT_FALSE(parse_shape_detection_json(json1, 0, &payload));
    TEST_ASSERT_FALSE(parse_shape_detection_json(json1, strlen(json1), NULL));
}

void test_build_shape_detection_packet(void) {
    uint8_t buffer[64];

    // Valid build
    int len = build_shape_detection_packet(SHAPE_TRIANGLE, 456, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_INT((int)(sizeof(FrameHeader) + sizeof(ShapeDetectionPayload)), len);

    // Validate using protocol unpacker
    EspNowPacket pkt = {};
    TEST_ASSERT_TRUE(unpack_packet(buffer, (size_t)len, &pkt));
    TEST_ASSERT_EQUAL_HEX8(ESPNOW_MAGIC_BYTE, pkt.header.magic);
    TEST_ASSERT_EQUAL_HEX8(OPCODE_SHAPE_DETECTION, pkt.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(sizeof(ShapeDetectionPayload), pkt.header.payload_len);
    TEST_ASSERT_EQUAL_UINT8(SHAPE_TRIANGLE, pkt.payload.shape_detection.shape_id);
    TEST_ASSERT_EQUAL_UINT32(456, pkt.payload.shape_detection.detection_id);

    // Validation errors
    TEST_ASSERT_EQUAL_INT(-1, build_shape_detection_packet(0, 1, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_INT(-1, build_shape_detection_packet(4, 1, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_INT(-1, build_shape_detection_packet(SHAPE_CIRCLE, 1, NULL, sizeof(buffer)));
    TEST_ASSERT_EQUAL_INT(-1, build_shape_detection_packet(SHAPE_CIRCLE, 1, buffer, 5)); // too small
}

void test_format_rollover_json(void) {
    BatchRolloverPayload rollover = {};
    char buf[128];

    // Circle rollover
    rollover.shape_id = SHAPE_CIRCLE;
    rollover.batch_size = 5;
    rollover.timestamp_ms = 12345;
    TEST_ASSERT_TRUE(format_rollover_json(&rollover, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"shape_id\":1,\"shape_name\":\"circle\",\"timestamp\":12345}", buf);

    // Triangle rollover
    rollover.shape_id = SHAPE_TRIANGLE;
    rollover.timestamp_ms = 99999;
    TEST_ASSERT_TRUE(format_rollover_json(&rollover, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"shape_id\":2,\"shape_name\":\"triangle\",\"timestamp\":99999}", buf);

    // Square rollover
    rollover.shape_id = SHAPE_SQUARE;
    rollover.timestamp_ms = 55555;
    TEST_ASSERT_TRUE(format_rollover_json(&rollover, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"shape_id\":3,\"shape_name\":\"square\",\"timestamp\":55555}", buf);

    // Errors
    TEST_ASSERT_FALSE(format_rollover_json(NULL, buf, sizeof(buf)));
    TEST_ASSERT_FALSE(format_rollover_json(&rollover, NULL, sizeof(buf)));
    TEST_ASSERT_FALSE(format_rollover_json(&rollover, buf, 0));
    char tiny_buf[10];
    TEST_ASSERT_FALSE(format_rollover_json(&rollover, tiny_buf, sizeof(tiny_buf)));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_parse_auth_response_ok);
    RUN_TEST(test_parse_auth_response_invalid_pin);
    RUN_TEST(test_parse_auth_response_user_locked);
    RUN_TEST(test_parse_auth_response_user_not_found);
    RUN_TEST(test_parse_auth_response_malformed_and_edge_cases);
    RUN_TEST(test_format_auth_request_json_valid);
    RUN_TEST(test_format_auth_request_json_buffer_too_small);
    RUN_TEST(test_build_beacon_packet_success);
    RUN_TEST(test_build_beacon_packet_validation_errors);
    RUN_TEST(test_network_status_helper);
    RUN_TEST(test_format_telemetry_json_valid);
    RUN_TEST(test_format_telemetry_json_boundary_and_null);
    RUN_TEST(test_parse_shape_detection_json);
    RUN_TEST(test_build_shape_detection_packet);
    RUN_TEST(test_format_rollover_json);
    return UNITY_END();
}



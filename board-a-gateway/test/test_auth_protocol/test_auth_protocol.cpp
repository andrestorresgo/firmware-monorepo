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
    return UNITY_END();
}

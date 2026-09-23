#include <unity.h>
#include <string.h>
#include "protocol.h"

void setUp(void) {
    // set up before each test
}

void tearDown(void) {
    // clean up after each test
}

void test_frame_header_magic_byte_defined(void) {
    TEST_ASSERT_EQUAL_HEX8(0xA5, ESPNOW_MAGIC_BYTE);
}

void test_checksum_calculation(void) {
    uint8_t sample_data[] = { 0xA5, 0x01, 0x04, 0x01, 0x02, 0x03, 0x04 };
    uint8_t checksum = calculate_checksum(sample_data, sizeof(sample_data));
    // Non-zero checksum for non-zero data
    TEST_ASSERT_NOT_EQUAL(0, checksum);
    
    // Checksum of identical data should match
    uint8_t checksum2 = calculate_checksum(sample_data, sizeof(sample_data));
    TEST_ASSERT_EQUAL_HEX8(checksum, checksum2);
}

void test_validate_frame_null_or_too_short(void) {
    TEST_ASSERT_FALSE(validate_frame(NULL, 0));
    TEST_ASSERT_FALSE(validate_frame(NULL, sizeof(FrameHeader)));
    
    uint8_t short_buf[sizeof(FrameHeader) - 1] = {0};
    TEST_ASSERT_FALSE(validate_frame(short_buf, sizeof(short_buf)));
}

void test_validate_frame_invalid_magic(void) {
    uint8_t buf[sizeof(FrameHeader) + 4] = {0};
    FrameHeader* hdr = (FrameHeader*)buf;
    hdr->magic = 0x5A; // Invalid magic (not 0xA5)
    hdr->opcode = OPCODE_BEACON;
    hdr->payload_len = 4;
    hdr->checksum = calculate_checksum(buf + sizeof(FrameHeader), 4);
    
    TEST_ASSERT_FALSE(validate_frame(buf, sizeof(buf)));
}

void test_pack_and_unpack_beacon_packet(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_BEACON;
    tx_packet.header.payload_len = sizeof(BeaconPayload);
    tx_packet.payload.beacon.wifi_channel = 6;
    tx_packet.payload.beacon.gateway_mac[0] = 0xAA;
    tx_packet.payload.beacon.gateway_mac[1] = 0xBB;
    tx_packet.payload.beacon.gateway_mac[2] = 0xCC;
    tx_packet.payload.beacon.gateway_mac[3] = 0xDD;
    tx_packet.payload.beacon.gateway_mac[4] = 0xEE;
    tx_packet.payload.beacon.gateway_mac[5] = 0xFF;
    tx_packet.payload.beacon.uptime_ms = 123456;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);
    TEST_ASSERT_EQUAL(sizeof(FrameHeader) + sizeof(BeaconPayload), (size_t)packed_len);

    // Frame validation must pass
    TEST_ASSERT_TRUE(validate_frame(buffer, packed_len));

    // Unpack and verify fields
    EspNowPacket rx_packet = {};
    bool unpack_ok = unpack_packet(buffer, packed_len, &rx_packet);
    TEST_ASSERT_TRUE(unpack_ok);
    TEST_ASSERT_EQUAL_HEX8(ESPNOW_MAGIC_BYTE, rx_packet.header.magic);
    TEST_ASSERT_EQUAL_HEX8(OPCODE_BEACON, rx_packet.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(sizeof(BeaconPayload), rx_packet.header.payload_len);
    TEST_ASSERT_EQUAL_UINT8(6, rx_packet.payload.beacon.wifi_channel);
    TEST_ASSERT_EQUAL_HEX8(0xAA, rx_packet.payload.beacon.gateway_mac[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, rx_packet.payload.beacon.gateway_mac[5]);
    TEST_ASSERT_EQUAL_UINT32(123456, rx_packet.payload.beacon.uptime_ms);
}

void test_pack_and_unpack_shape_detection(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_SHAPE_DETECTION;
    tx_packet.header.payload_len = sizeof(ShapeDetectionPayload);
    tx_packet.payload.shape_detection.shape_id = SHAPE_TRIANGLE;
    tx_packet.payload.shape_detection.detection_id = 42;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);

    EspNowPacket rx_packet = {};
    TEST_ASSERT_TRUE(unpack_packet(buffer, packed_len, &rx_packet));
    TEST_ASSERT_EQUAL_HEX8(OPCODE_SHAPE_DETECTION, rx_packet.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(SHAPE_TRIANGLE, rx_packet.payload.shape_detection.shape_id);
    TEST_ASSERT_EQUAL_UINT32(42, rx_packet.payload.shape_detection.detection_id);
}

void test_pack_and_unpack_telemetry(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_TELEMETRY;
    tx_packet.header.payload_len = sizeof(TelemetryPayload);
    tx_packet.payload.telemetry.is_paused = 1;
    tx_packet.payload.telemetry.motor_state = 0;
    tx_packet.payload.telemetry.servo_state = SERVO_OPEN;
    tx_packet.payload.telemetry.red_count = 5;
    tx_packet.payload.telemetry.green_count = 2;
    tx_packet.payload.telemetry.blue_count = 0;
    tx_packet.payload.telemetry.uptime_ms = 99999;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);

    EspNowPacket rx_packet = {};
    TEST_ASSERT_TRUE(unpack_packet(buffer, packed_len, &rx_packet));
    TEST_ASSERT_EQUAL_HEX8(OPCODE_TELEMETRY, rx_packet.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(1, rx_packet.payload.telemetry.is_paused);
    TEST_ASSERT_EQUAL_UINT8(0, rx_packet.payload.telemetry.motor_state);
    TEST_ASSERT_EQUAL_UINT8(SERVO_OPEN, rx_packet.payload.telemetry.servo_state);
    TEST_ASSERT_EQUAL_UINT8(5, rx_packet.payload.telemetry.red_count);
    TEST_ASSERT_EQUAL_UINT8(2, rx_packet.payload.telemetry.green_count);
    TEST_ASSERT_EQUAL_UINT8(0, rx_packet.payload.telemetry.blue_count);
    TEST_ASSERT_EQUAL_UINT32(99999, rx_packet.payload.telemetry.uptime_ms);
}

void test_pack_and_unpack_batch_rollover(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_BATCH_ROLLOVER;
    tx_packet.header.payload_len = sizeof(BatchRolloverPayload);
    tx_packet.payload.batch_rollover.shape_id = SHAPE_CIRCLE;
    tx_packet.payload.batch_rollover.batch_size = 5;
    tx_packet.payload.batch_rollover.timestamp_ms = 8888;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);

    EspNowPacket rx_packet = {};
    TEST_ASSERT_TRUE(unpack_packet(buffer, packed_len, &rx_packet));
    TEST_ASSERT_EQUAL_HEX8(OPCODE_BATCH_ROLLOVER, rx_packet.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(SHAPE_CIRCLE, rx_packet.payload.batch_rollover.shape_id);
    TEST_ASSERT_EQUAL_UINT8(5, rx_packet.payload.batch_rollover.batch_size);
    TEST_ASSERT_EQUAL_UINT32(8888, rx_packet.payload.batch_rollover.timestamp_ms);
}

void test_pack_and_unpack_servo_command(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_SERVO_COMMAND;
    tx_packet.header.payload_len = sizeof(ServoCommandPayload);
    tx_packet.payload.servo_command.servo_state = SERVO_CLOSED;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);

    EspNowPacket rx_packet = {};
    TEST_ASSERT_TRUE(unpack_packet(buffer, packed_len, &rx_packet));
    TEST_ASSERT_EQUAL_HEX8(OPCODE_SERVO_COMMAND, rx_packet.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, rx_packet.payload.servo_command.servo_state);
}

void test_corrupted_checksum_rejected(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_SERVO_COMMAND;
    tx_packet.header.payload_len = sizeof(ServoCommandPayload);
    tx_packet.payload.servo_command.servo_state = SERVO_OPEN;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);

    // Corrupt one byte of payload
    buffer[sizeof(FrameHeader)] ^= 0xFF;

    TEST_ASSERT_FALSE(validate_frame(buffer, packed_len));

    EspNowPacket rx_packet = {};
    TEST_ASSERT_FALSE(unpack_packet(buffer, packed_len, &rx_packet));
}

void test_struct_packing_invariants(void) {
    // Ensure packed structs do not have unexpected padding
    TEST_ASSERT_EQUAL(4, sizeof(FrameHeader));
    TEST_ASSERT_EQUAL(11, sizeof(BeaconPayload)); // 1 + 6 + 4
    TEST_ASSERT_EQUAL(7, sizeof(BeaconAckPayload)); // 6 + 1
    TEST_ASSERT_EQUAL(5, sizeof(ShapeDetectionPayload)); // 1 + 4
    TEST_ASSERT_EQUAL(1, sizeof(ServoCommandPayload)); // 1
    TEST_ASSERT_EQUAL(10, sizeof(TelemetryPayload)); // 1 + 1 + 1 + 1 + 1 + 1 + 4
    TEST_ASSERT_EQUAL(6, sizeof(BatchRolloverPayload)); // 1 + 1 + 4
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_frame_header_magic_byte_defined);
    RUN_TEST(test_checksum_calculation);
    RUN_TEST(test_validate_frame_null_or_too_short);
    RUN_TEST(test_validate_frame_invalid_magic);
    RUN_TEST(test_pack_and_unpack_beacon_packet);
    RUN_TEST(test_pack_and_unpack_shape_detection);
    RUN_TEST(test_pack_and_unpack_telemetry);
    RUN_TEST(test_pack_and_unpack_batch_rollover);
    RUN_TEST(test_pack_and_unpack_servo_command);
    RUN_TEST(test_corrupted_checksum_rejected);
    RUN_TEST(test_struct_packing_invariants);
    return UNITY_END();
}

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
    TEST_ASSERT_NOT_EQUAL(0, checksum);
    
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

void test_pack_and_unpack_beacon_ack_packet(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_BEACON_ACK;
    tx_packet.header.payload_len = sizeof(BeaconAckPayload);
    tx_packet.payload.beacon_ack.actuator_mac[0] = 0x11;
    tx_packet.payload.beacon_ack.actuator_mac[1] = 0x22;
    tx_packet.payload.beacon_ack.actuator_mac[2] = 0x33;
    tx_packet.payload.beacon_ack.actuator_mac[3] = 0x44;
    tx_packet.payload.beacon_ack.actuator_mac[4] = 0x55;
    tx_packet.payload.beacon_ack.actuator_mac[5] = 0x66;
    tx_packet.payload.beacon_ack.status = 0;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);
    TEST_ASSERT_EQUAL(sizeof(FrameHeader) + sizeof(BeaconAckPayload), (size_t)packed_len);

    TEST_ASSERT_TRUE(validate_frame(buffer, packed_len));

    EspNowPacket rx_packet = {};
    bool unpack_ok = unpack_packet(buffer, packed_len, &rx_packet);
    TEST_ASSERT_TRUE(unpack_ok);
    TEST_ASSERT_EQUAL_HEX8(OPCODE_BEACON_ACK, rx_packet.header.opcode);
    TEST_ASSERT_EQUAL_HEX8(0x11, rx_packet.payload.beacon_ack.actuator_mac[0]);
    TEST_ASSERT_EQUAL_HEX8(0x66, rx_packet.payload.beacon_ack.actuator_mac[5]);
    TEST_ASSERT_EQUAL_UINT8(0, rx_packet.payload.beacon_ack.status);
}

void test_pack_and_unpack_shape_detection(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_SHAPE_DETECTION;
    tx_packet.header.payload_len = sizeof(ShapeDetectionPayload);
    tx_packet.payload.shape_detection.shape_id = SHAPE_SQUARE;
    tx_packet.payload.shape_detection.detection_id = 101;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);

    EspNowPacket rx_packet = {};
    TEST_ASSERT_TRUE(unpack_packet(buffer, packed_len, &rx_packet));
    TEST_ASSERT_EQUAL_HEX8(OPCODE_SHAPE_DETECTION, rx_packet.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(SHAPE_SQUARE, rx_packet.payload.shape_detection.shape_id);
    TEST_ASSERT_EQUAL_UINT32(101, rx_packet.payload.shape_detection.detection_id);
}

void test_pack_and_unpack_telemetry(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_TELEMETRY;
    tx_packet.header.payload_len = sizeof(TelemetryPayload);
    tx_packet.payload.telemetry.is_paused = 0;
    tx_packet.payload.telemetry.motor_state = 1;
    tx_packet.payload.telemetry.servo_state = SERVO_CLOSED;
    tx_packet.payload.telemetry.red_count = 1;
    tx_packet.payload.telemetry.green_count = 3;
    tx_packet.payload.telemetry.blue_count = 4;
    tx_packet.payload.telemetry.uptime_ms = 54321;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);

    EspNowPacket rx_packet = {};
    TEST_ASSERT_TRUE(unpack_packet(buffer, packed_len, &rx_packet));
    TEST_ASSERT_EQUAL_HEX8(OPCODE_TELEMETRY, rx_packet.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(0, rx_packet.payload.telemetry.is_paused);
    TEST_ASSERT_EQUAL_UINT8(1, rx_packet.payload.telemetry.motor_state);
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, rx_packet.payload.telemetry.servo_state);
    TEST_ASSERT_EQUAL_UINT8(1, rx_packet.payload.telemetry.red_count);
    TEST_ASSERT_EQUAL_UINT8(3, rx_packet.payload.telemetry.green_count);
    TEST_ASSERT_EQUAL_UINT8(4, rx_packet.payload.telemetry.blue_count);
    TEST_ASSERT_EQUAL_UINT32(54321, rx_packet.payload.telemetry.uptime_ms);
}

void test_pack_and_unpack_batch_rollover(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_BATCH_ROLLOVER;
    tx_packet.header.payload_len = sizeof(BatchRolloverPayload);
    tx_packet.payload.batch_rollover.shape_id = SHAPE_TRIANGLE;
    tx_packet.payload.batch_rollover.batch_size = 5;
    tx_packet.payload.batch_rollover.timestamp_ms = 7777;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);

    EspNowPacket rx_packet = {};
    TEST_ASSERT_TRUE(unpack_packet(buffer, packed_len, &rx_packet));
    TEST_ASSERT_EQUAL_HEX8(OPCODE_BATCH_ROLLOVER, rx_packet.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(SHAPE_TRIANGLE, rx_packet.payload.batch_rollover.shape_id);
    TEST_ASSERT_EQUAL_UINT8(5, rx_packet.payload.batch_rollover.batch_size);
    TEST_ASSERT_EQUAL_UINT32(7777, rx_packet.payload.batch_rollover.timestamp_ms);
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

    buffer[sizeof(FrameHeader)] ^= 0xFF; // Corrupt payload byte

    TEST_ASSERT_FALSE(validate_frame(buffer, packed_len));

    EspNowPacket rx_packet = {};
    TEST_ASSERT_FALSE(unpack_packet(buffer, packed_len, &rx_packet));
}

void test_pack_and_unpack_motor_command(void) {
    EspNowPacket tx_packet = {};
    tx_packet.header.magic = ESPNOW_MAGIC_BYTE;
    tx_packet.header.opcode = OPCODE_MOTOR_COMMAND;
    tx_packet.header.payload_len = sizeof(MotorCommandPayload);
    tx_packet.payload.motor_command.motor_state = MOTOR_MEDIUM;

    uint8_t buffer[64];
    int packed_len = pack_packet(&tx_packet, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, packed_len);
    TEST_ASSERT_EQUAL(sizeof(FrameHeader) + sizeof(MotorCommandPayload), packed_len);

    TEST_ASSERT_TRUE(validate_frame(buffer, packed_len));

    EspNowPacket rx_packet = {};
    TEST_ASSERT_TRUE(unpack_packet(buffer, packed_len, &rx_packet));
    TEST_ASSERT_EQUAL_HEX8(OPCODE_MOTOR_COMMAND, rx_packet.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(MOTOR_MEDIUM, rx_packet.payload.motor_command.motor_state);
}

void test_struct_packing_invariants(void) {
    TEST_ASSERT_EQUAL(4, sizeof(FrameHeader));
    TEST_ASSERT_EQUAL(11, sizeof(BeaconPayload));
    TEST_ASSERT_EQUAL(7, sizeof(BeaconAckPayload));
    TEST_ASSERT_EQUAL(5, sizeof(ShapeDetectionPayload));
    TEST_ASSERT_EQUAL(1, sizeof(ServoCommandPayload));
    TEST_ASSERT_EQUAL(1, sizeof(MotorCommandPayload));
    TEST_ASSERT_EQUAL(10, sizeof(TelemetryPayload));
    TEST_ASSERT_EQUAL(6, sizeof(BatchRolloverPayload));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_frame_header_magic_byte_defined);
    RUN_TEST(test_checksum_calculation);
    RUN_TEST(test_validate_frame_null_or_too_short);
    RUN_TEST(test_validate_frame_invalid_magic);
    RUN_TEST(test_pack_and_unpack_beacon_ack_packet);
    RUN_TEST(test_pack_and_unpack_shape_detection);
    RUN_TEST(test_pack_and_unpack_telemetry);
    RUN_TEST(test_pack_and_unpack_batch_rollover);
    RUN_TEST(test_pack_and_unpack_motor_command);
    RUN_TEST(test_corrupted_checksum_rejected);
    RUN_TEST(test_struct_packing_invariants);
    return UNITY_END();
}


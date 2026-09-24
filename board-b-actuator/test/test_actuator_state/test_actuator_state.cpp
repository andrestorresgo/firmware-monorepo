#include <unity.h>
#include "actuator_state.h"
#include "protocol.h"

void setUp(void) {}
void tearDown(void) {}

void test_initial_state_defaults(void) {
    ActuatorState state;
    TEST_ASSERT_FALSE(state.is_paused());
    TEST_ASSERT_TRUE(state.get_motor_state());
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, state.get_servo_state());
    TEST_ASSERT_EQUAL_UINT8(0, state.get_red_count());
    TEST_ASSERT_EQUAL_UINT8(0, state.get_green_count());
    TEST_ASSERT_EQUAL_UINT8(0, state.get_blue_count());
    TEST_ASSERT_FALSE(state.is_dirty());
}

void test_pause_lockout_and_resume(void) {
    ActuatorState state;
    
    // Toggle pause ON
    state.set_paused(true);
    TEST_ASSERT_TRUE(state.is_paused());
    TEST_ASSERT_FALSE(state.get_motor_state()); // Motor must be cut to 0
    TEST_ASSERT_TRUE(state.is_dirty());
    state.clear_dirty();

    // While paused, attempt to run motor or move servo or increment counts
    TEST_ASSERT_FALSE(state.set_motor_state(true));
    TEST_ASSERT_FALSE(state.get_motor_state()); // Remains locked false

    TEST_ASSERT_FALSE(state.set_servo_state(SERVO_OPEN));
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, state.get_servo_state()); // Remains closed

    TEST_ASSERT_FALSE(state.set_shape_count(SHAPE_CIRCLE, 3));
    TEST_ASSERT_EQUAL_UINT8(0, state.get_red_count()); // Remains 0

    // Toggle pause OFF (resume)
    state.set_paused(false);
    TEST_ASSERT_FALSE(state.is_paused());
    TEST_ASSERT_TRUE(state.get_motor_state()); // Motor restored
    TEST_ASSERT_TRUE(state.is_dirty());
}

void test_normal_state_mutations_and_dirty_flag(void) {
    ActuatorState state;
    state.clear_dirty();

    TEST_ASSERT_TRUE(state.set_servo_state(SERVO_OPEN));
    TEST_ASSERT_EQUAL_UINT8(SERVO_OPEN, state.get_servo_state());
    TEST_ASSERT_TRUE(state.is_dirty());
    state.clear_dirty();

    // Setting same servo state should not mark dirty
    TEST_ASSERT_TRUE(state.set_servo_state(SERVO_OPEN));
    TEST_ASSERT_FALSE(state.is_dirty());

    TEST_ASSERT_TRUE(state.set_shape_count(SHAPE_CIRCLE, 4));
    TEST_ASSERT_EQUAL_UINT8(4, state.get_red_count());
    TEST_ASSERT_TRUE(state.is_dirty());
    state.clear_dirty();

    TEST_ASSERT_TRUE(state.set_shape_count(SHAPE_TRIANGLE, 5));
    TEST_ASSERT_EQUAL_UINT8(5, state.get_green_count());
    TEST_ASSERT_TRUE(state.is_dirty());
    state.clear_dirty();

    TEST_ASSERT_TRUE(state.set_shape_count(SHAPE_SQUARE, 2));
    TEST_ASSERT_EQUAL_UINT8(2, state.get_blue_count());
    TEST_ASSERT_TRUE(state.is_dirty());
    state.clear_dirty();

    // Invalid shape ID should fail
    TEST_ASSERT_FALSE(state.set_shape_count(99, 1));
}

void test_telemetry_payload_generation(void) {
    ActuatorState state;
    state.set_shape_count(SHAPE_CIRCLE, 1);
    state.set_shape_count(SHAPE_TRIANGLE, 2);
    state.set_shape_count(SHAPE_SQUARE, 3);
    state.set_servo_state(SERVO_OPEN);

    TelemetryPayload payload = {};
    state.build_telemetry_payload(98765, &payload);

    TEST_ASSERT_EQUAL_UINT8(0, payload.is_paused);
    TEST_ASSERT_EQUAL_UINT8(1, payload.motor_state);
    TEST_ASSERT_EQUAL_UINT8(SERVO_OPEN, payload.servo_state);
    TEST_ASSERT_EQUAL_UINT8(1, payload.red_count);
    TEST_ASSERT_EQUAL_UINT8(2, payload.green_count);
    TEST_ASSERT_EQUAL_UINT8(3, payload.blue_count);
    TEST_ASSERT_EQUAL_UINT32(98765, payload.uptime_ms);
}

void test_telemetry_packet_generation(void) {
    ActuatorState state;
    state.set_shape_count(SHAPE_CIRCLE, 5);
    state.set_paused(true);

    EspNowPacket pkt = {};

    int len = state.build_telemetry_packet(112233, &pkt);

    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_HEX8(ESPNOW_MAGIC_BYTE, pkt.header.magic);
    TEST_ASSERT_EQUAL_HEX8(OPCODE_TELEMETRY, pkt.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(sizeof(TelemetryPayload), pkt.header.payload_len);
    TEST_ASSERT_EQUAL_UINT8(1, pkt.payload.telemetry.is_paused);
    TEST_ASSERT_EQUAL_UINT8(0, pkt.payload.telemetry.motor_state);
    TEST_ASSERT_EQUAL_UINT8(5, pkt.payload.telemetry.red_count);
    TEST_ASSERT_EQUAL_UINT32(112233, pkt.payload.telemetry.uptime_ms);

    // Verify it packs and validates using protocol functions
    uint8_t raw[64];
    int packed_len = pack_packet(&pkt, raw, sizeof(raw));
    TEST_ASSERT_GREATER_THAN(0, packed_len);
    TEST_ASSERT_TRUE(validate_frame(raw, (size_t)packed_len));
}

void test_motor_speed_states_and_pause_recovery(void) {
    ActuatorState state;
    TEST_ASSERT_EQUAL_UINT8(MOTOR_ON, state.get_motor_state());

    // Switch to MEDIUM
    TEST_ASSERT_TRUE(state.set_motor_state(MOTOR_MEDIUM));
    TEST_ASSERT_EQUAL_UINT8(MOTOR_MEDIUM, state.get_motor_state());
    TEST_ASSERT_TRUE(state.is_dirty());
    state.clear_dirty();

    // Redundant setting does not mark dirty
    TEST_ASSERT_TRUE(state.set_motor_state(MOTOR_MEDIUM));
    TEST_ASSERT_FALSE(state.is_dirty());

    // Switch to OFF
    TEST_ASSERT_TRUE(state.set_motor_state(MOTOR_OFF));
    TEST_ASSERT_EQUAL_UINT8(MOTOR_OFF, state.get_motor_state());
    TEST_ASSERT_TRUE(state.is_dirty());
    state.clear_dirty();

    // Invalid state returns false
    TEST_ASSERT_FALSE(state.set_motor_state(99));

    // Restore to MEDIUM
    TEST_ASSERT_TRUE(state.set_motor_state(MOTOR_MEDIUM));

    // Pause engaging forces motor to OFF (lockout)
    state.set_paused(true);
    TEST_ASSERT_EQUAL_UINT8(MOTOR_OFF, state.get_motor_state());

    // While paused, changing motor speed state is rejected
    TEST_ASSERT_FALSE(state.set_motor_state(MOTOR_ON));
    TEST_ASSERT_EQUAL_UINT8(MOTOR_OFF, state.get_motor_state());

    // Resuming restores pre-pause state (MOTOR_MEDIUM)
    state.set_paused(false);
    TEST_ASSERT_EQUAL_UINT8(MOTOR_MEDIUM, state.get_motor_state());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_defaults);
    RUN_TEST(test_pause_lockout_and_resume);
    RUN_TEST(test_normal_state_mutations_and_dirty_flag);
    RUN_TEST(test_motor_speed_states_and_pause_recovery);
    RUN_TEST(test_telemetry_payload_generation);
    RUN_TEST(test_telemetry_packet_generation);
    return UNITY_END();
}

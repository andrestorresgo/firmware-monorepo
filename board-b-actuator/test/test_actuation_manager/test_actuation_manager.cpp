#include <unity.h>
#include "actuator_state.h"
#include "hbridge_motor.h"
#include "servo_gate_driver.h"
#include "button_driver.h"
#include "actuation_manager.h"
#include "counting_manager.h"

void setUp(void) {}
void tearDown(void) {}

void test_hbridge_motor_mock(void) {
    MockHBridgeMotor motor;
    TEST_ASSERT_FALSE(motor.was_begin_called());
    TEST_ASSERT_FALSE(motor.is_running());
    TEST_ASSERT_EQUAL_UINT8(0, motor.get_duty_percent());

    motor.begin();
    TEST_ASSERT_TRUE(motor.was_begin_called());

    // Drive forward 80% duty cycle per Ticket 06
    motor.drive_forward(80);
    TEST_ASSERT_TRUE(motor.is_running());
    TEST_ASSERT_EQUAL_UINT8(80, motor.get_duty_percent());

    // Stop motor (cut power)
    motor.stop();
    TEST_ASSERT_FALSE(motor.is_running());
    TEST_ASSERT_EQUAL_UINT8(0, motor.get_duty_percent());

    // Duty percent clamped to 100
    motor.drive_forward(150);
    TEST_ASSERT_EQUAL_UINT8(100, motor.get_duty_percent());
}

void test_servo_gate_driver_mock(void) {
    MockServoGateDriver servo;
    TEST_ASSERT_FALSE(servo.was_begin_called());

    servo.begin();
    TEST_ASSERT_TRUE(servo.was_begin_called());
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, servo.get_position());
    TEST_ASSERT_EQUAL_INT(0, servo.get_angle());

    // Open gate to 90 degrees
    servo.set_position(SERVO_OPEN);
    TEST_ASSERT_EQUAL_UINT8(SERVO_OPEN, servo.get_position());
    TEST_ASSERT_EQUAL_INT(90, servo.get_angle());

    // Close gate to 0 degrees
    servo.set_position(SERVO_CLOSED);
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, servo.get_position());
    TEST_ASSERT_EQUAL_INT(0, servo.get_angle());
}

void test_debounced_button_timing_and_noise(void) {
    DebouncedButton btn(50); // 50ms software debounce
    TEST_ASSERT_FALSE(btn.is_pressed());
    TEST_ASSERT_EQUAL_UINT32(50, btn.get_debounce_ms());

    // Initial unpressed
    TEST_ASSERT_FALSE(btn.update(0, false));

    // Press starts at t=10
    TEST_ASSERT_FALSE(btn.update(10, true));
    TEST_ASSERT_FALSE(btn.is_pressed());

    // 20ms later (t=30), still within debounce window (<50ms)
    TEST_ASSERT_FALSE(btn.update(30, true));
    TEST_ASSERT_FALSE(btn.is_pressed());

    // Noise glitch at t=40 (releases momentarily)
    TEST_ASSERT_FALSE(btn.update(40, false));
    TEST_ASSERT_FALSE(btn.is_pressed());

    // Pressed again at t=50 (debounce timer resets to 50)
    TEST_ASSERT_FALSE(btn.update(50, true));
    TEST_ASSERT_FALSE(btn.is_pressed());

    // t=90 (40ms elapsed since reset)
    TEST_ASSERT_FALSE(btn.update(90, true));
    TEST_ASSERT_FALSE(btn.is_pressed());

    // t=105 (55ms elapsed >= 50ms) -> Triggers edge!
    TEST_ASSERT_TRUE(btn.update(105, true));
    TEST_ASSERT_TRUE(btn.is_pressed());

    // t=120, held continuously -> no re-trigger
    TEST_ASSERT_FALSE(btn.update(120, true));
    TEST_ASSERT_TRUE(btn.is_pressed());

    // Release at t=200
    TEST_ASSERT_FALSE(btn.update(200, false));
    TEST_ASSERT_TRUE(btn.is_pressed()); // Still true until release debounced

    // Release stable for 50ms at t=255
    TEST_ASSERT_FALSE(btn.update(255, false));
    TEST_ASSERT_FALSE(btn.is_pressed());
}

void test_actuation_manager_normal_operation(void) {
    ActuatorState state;
    MockHBridgeMotor motor;
    MockServoGateDriver servo;
    DebouncedButton button(50);
    ActuationManager manager(state, motor, servo, button);

    manager.begin();
    state.clear_dirty();

    // Motor should be driven forward at 80% duty cycle
    TEST_ASSERT_TRUE(manager.is_motor_running());
    TEST_ASSERT_EQUAL_UINT8(80, manager.get_motor_duty());
    TEST_ASSERT_TRUE(state.get_motor_state());

    // Servo should be Closed at 0 degrees
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, manager.get_servo_state());
    TEST_ASSERT_EQUAL_INT(0, manager.get_servo_angle());
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, state.get_servo_state());

    // Command servo to OPEN
    bool transition = manager.handle_servo_command(SERVO_OPEN);
    TEST_ASSERT_TRUE(transition);
    TEST_ASSERT_EQUAL_UINT8(SERVO_OPEN, manager.get_servo_state());
    TEST_ASSERT_EQUAL_INT(90, manager.get_servo_angle());
    TEST_ASSERT_EQUAL_UINT8(SERVO_OPEN, state.get_servo_state());
    TEST_ASSERT_TRUE(state.is_dirty());
    state.clear_dirty();

    // Command servo to OPEN again (no transition)
    transition = manager.handle_servo_command(SERVO_OPEN);
    TEST_ASSERT_FALSE(transition);
    TEST_ASSERT_FALSE(state.is_dirty());

    // Command servo to CLOSED
    transition = manager.handle_servo_command(SERVO_CLOSED);
    TEST_ASSERT_TRUE(transition);
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, manager.get_servo_state());
    TEST_ASSERT_EQUAL_INT(0, manager.get_servo_angle());
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, state.get_servo_state());
    TEST_ASSERT_TRUE(state.is_dirty());

    // Invalid command rejected
    TEST_ASSERT_FALSE(manager.handle_servo_command(99));
}

void test_machine_pause_hardware_lockout(void) {
    ActuatorState state;
    MockHBridgeMotor motor;
    MockServoGateDriver servo;
    DebouncedButton button(50);
    ActuationManager manager(state, motor, servo, button);

    manager.begin();
    state.clear_dirty();

    TEST_ASSERT_FALSE(manager.is_paused());
    TEST_ASSERT_TRUE(manager.is_motor_running());
    TEST_ASSERT_EQUAL_UINT8(80, manager.get_motor_duty());

    // Step 1: Simulate physical push button press on GPIO 25
    manager.handle_pause_button(0, true);
    bool toggled = manager.handle_pause_button(60, true);
    TEST_ASSERT_TRUE(toggled);

    // Entering Machine Pause:
    // Total hardware lockout per ADR-0004
    TEST_ASSERT_TRUE(manager.is_paused());
    TEST_ASSERT_TRUE(state.is_paused());
    TEST_ASSERT_TRUE(state.is_dirty());
    state.clear_dirty();

    // DC motor PWM immediately cut to 0%
    TEST_ASSERT_FALSE(manager.is_motor_running());
    TEST_ASSERT_EQUAL_UINT8(0, manager.get_motor_duty());
    TEST_ASSERT_FALSE(state.get_motor_state());

    // Servo commands suppressed during Machine Pause
    bool servo_cmd_accepted = manager.handle_servo_command(SERVO_OPEN);
    TEST_ASSERT_FALSE(servo_cmd_accepted);
    TEST_ASSERT_EQUAL_UINT8(SERVO_CLOSED, manager.get_servo_state());
    TEST_ASSERT_EQUAL_INT(0, manager.get_servo_angle());
    TEST_ASSERT_FALSE(state.is_dirty());

    // Shape detection increments blocked during Machine Pause
    MockLedBankDriver led_driver;
    CountingManager counting_mgr(state, led_driver);
    counting_mgr.begin();
    TEST_ASSERT_FALSE(counting_mgr.handle_shape_detection(SHAPE_CIRCLE, 100));
    TEST_ASSERT_EQUAL_UINT8(0, state.get_red_count());

    // Release button
    manager.handle_pause_button(100, false);
    manager.handle_pause_button(160, false);

    // Step 2: Press button again to exit Machine Pause
    manager.handle_pause_button(200, true);
    toggled = manager.handle_pause_button(260, true);
    TEST_ASSERT_TRUE(toggled);

    // Exiting Machine Pause:
    TEST_ASSERT_FALSE(manager.is_paused());
    TEST_ASSERT_FALSE(state.is_paused());
    TEST_ASSERT_TRUE(state.is_dirty());

    // Restores DC motor power to 80% duty cycle
    TEST_ASSERT_TRUE(manager.is_motor_running());
    TEST_ASSERT_EQUAL_UINT8(80, manager.get_motor_duty());
    TEST_ASSERT_TRUE(state.get_motor_state());

    // Servo commands unlocked
    TEST_ASSERT_TRUE(manager.handle_servo_command(SERVO_OPEN));
    TEST_ASSERT_EQUAL_UINT8(SERVO_OPEN, manager.get_servo_state());
    TEST_ASSERT_EQUAL_INT(90, manager.get_servo_angle());

    // Shape detection unlocked
    TEST_ASSERT_TRUE(counting_mgr.handle_shape_detection(SHAPE_CIRCLE, 300));
    TEST_ASSERT_EQUAL_UINT8(1, counting_mgr.get_count(SHAPE_CIRCLE));
}

void test_pause_preserves_led_binary_counts(void) {
    ActuatorState state;
    MockHBridgeMotor motor;
    MockServoGateDriver servo;
    DebouncedButton button(50);
    ActuationManager manager(state, motor, servo, button);
    MockLedBankDriver led_driver;
    CountingManager counting_mgr(state, led_driver);

    manager.begin();
    counting_mgr.begin();

    // Accumulate shape counts
    for (int i = 0; i < 3; ++i) counting_mgr.handle_shape_detection(SHAPE_CIRCLE, i * 10);
    for (int i = 0; i < 2; ++i) counting_mgr.handle_shape_detection(SHAPE_TRIANGLE, i * 10);
    for (int i = 0; i < 4; ++i) counting_mgr.handle_shape_detection(SHAPE_SQUARE, i * 10);

    TEST_ASSERT_EQUAL_UINT8(3, counting_mgr.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(2, counting_mgr.get_count(SHAPE_TRIANGLE));
    TEST_ASSERT_EQUAL_UINT8(4, counting_mgr.get_count(SHAPE_SQUARE));
    TEST_ASSERT_EQUAL_UINT8(3, led_driver.get_bank_pattern(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(2, led_driver.get_bank_pattern(SHAPE_TRIANGLE));
    TEST_ASSERT_EQUAL_UINT8(4, led_driver.get_bank_pattern(SHAPE_SQUARE));

    // Toggle pause ON
    manager.handle_pause_button(100, true);
    TEST_ASSERT_TRUE(manager.handle_pause_button(160, true));
    TEST_ASSERT_TRUE(state.is_paused());

    // Verify existing binary counts on all LED banks are strictly preserved
    TEST_ASSERT_EQUAL_UINT8(3, state.get_red_count());
    TEST_ASSERT_EQUAL_UINT8(2, state.get_green_count());
    TEST_ASSERT_EQUAL_UINT8(4, state.get_blue_count());
    TEST_ASSERT_EQUAL_UINT8(3, led_driver.get_bank_pattern(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(2, led_driver.get_bank_pattern(SHAPE_TRIANGLE));
    TEST_ASSERT_EQUAL_UINT8(4, led_driver.get_bank_pattern(SHAPE_SQUARE));

    // Release button
    manager.handle_pause_button(200, false);
    manager.handle_pause_button(260, false);

    // Toggle pause OFF
    manager.handle_pause_button(300, true);
    TEST_ASSERT_TRUE(manager.handle_pause_button(360, true));
    TEST_ASSERT_FALSE(state.is_paused());

    // Verify existing binary counts are still preserved after resume
    TEST_ASSERT_EQUAL_UINT8(3, state.get_red_count());
    TEST_ASSERT_EQUAL_UINT8(2, state.get_green_count());
    TEST_ASSERT_EQUAL_UINT8(4, state.get_blue_count());
    TEST_ASSERT_EQUAL_UINT8(3, led_driver.get_bank_pattern(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(2, led_driver.get_bank_pattern(SHAPE_TRIANGLE));
    TEST_ASSERT_EQUAL_UINT8(4, led_driver.get_bank_pattern(SHAPE_SQUARE));
}

void test_telemetry_packet_generation_on_actuation_transitions(void) {
    ActuatorState state;
    MockHBridgeMotor motor;
    MockServoGateDriver servo;
    DebouncedButton button(50);
    ActuationManager manager(state, motor, servo, button);

    manager.begin();
    manager.handle_servo_command(SERVO_OPEN);

    struct EspNowPacket pkt;
    int len = state.build_telemetry_packet(12345, &pkt);
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_UINT8(OPCODE_TELEMETRY, pkt.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(0, pkt.payload.telemetry.is_paused);
    TEST_ASSERT_EQUAL_UINT8(1, pkt.payload.telemetry.motor_state);
    TEST_ASSERT_EQUAL_UINT8(SERVO_OPEN, pkt.payload.telemetry.servo_state);

    // Toggle to pause
    manager.handle_pause_button(0, true);
    manager.handle_pause_button(60, true);

    len = state.build_telemetry_packet(12400, &pkt);
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_UINT8(1, pkt.payload.telemetry.is_paused);
    TEST_ASSERT_EQUAL_UINT8(0, pkt.payload.telemetry.motor_state);
    TEST_ASSERT_EQUAL_UINT8(SERVO_OPEN, pkt.payload.telemetry.servo_state);
}

void test_actuation_manager_motor_command(void) {
    ActuatorState state;
    MockHBridgeMotor motor;
    MockServoGateDriver servo;
    DebouncedButton button(50);
    ActuationManager manager(state, motor, servo, button);

    manager.begin();
    TEST_ASSERT_EQUAL_UINT8(80, manager.get_motor_duty());
    TEST_ASSERT_EQUAL_UINT8(MOTOR_ON, state.get_motor_state());

    // Command to MEDIUM (50% duty cycle)
    bool res = manager.handle_motor_command(MOTOR_MEDIUM);
    TEST_ASSERT_TRUE(res);
    TEST_ASSERT_EQUAL_UINT8(50, manager.get_motor_duty());
    TEST_ASSERT_TRUE(manager.is_motor_running());
    TEST_ASSERT_EQUAL_UINT8(MOTOR_MEDIUM, state.get_motor_state());

    // Redundant command rejected
    res = manager.handle_motor_command(MOTOR_MEDIUM);
    TEST_ASSERT_FALSE(res);

    // Command to OFF (0% duty cycle)
    res = manager.handle_motor_command(MOTOR_OFF);
    TEST_ASSERT_TRUE(res);
    TEST_ASSERT_EQUAL_UINT8(0, manager.get_motor_duty());
    TEST_ASSERT_FALSE(manager.is_motor_running());
    TEST_ASSERT_EQUAL_UINT8(MOTOR_OFF, state.get_motor_state());

    // Command to ON (80% duty cycle)
    res = manager.handle_motor_command(MOTOR_ON);
    TEST_ASSERT_TRUE(res);
    TEST_ASSERT_EQUAL_UINT8(80, manager.get_motor_duty());
    TEST_ASSERT_TRUE(manager.is_motor_running());
    TEST_ASSERT_EQUAL_UINT8(MOTOR_ON, state.get_motor_state());

    // Invalid command rejected
    TEST_ASSERT_FALSE(manager.handle_motor_command(99));

    // Switch to MEDIUM, then Pause
    manager.handle_motor_command(MOTOR_MEDIUM);
    TEST_ASSERT_EQUAL_UINT8(50, manager.get_motor_duty());

    // Pause toggle
    manager.handle_pause_button(0, true);
    manager.handle_pause_button(60, true);
    TEST_ASSERT_TRUE(manager.is_paused());
    TEST_ASSERT_EQUAL_UINT8(0, manager.get_motor_duty());

    // While paused, remote motor command rejected
    TEST_ASSERT_FALSE(manager.handle_motor_command(MOTOR_ON));
    TEST_ASSERT_EQUAL_UINT8(0, manager.get_motor_duty());

    // Release button
    manager.handle_pause_button(100, false);
    manager.handle_pause_button(160, false);

    // Resume from pause restores MEDIUM (50% duty cycle)
    manager.handle_pause_button(200, true);
    bool toggled_resume = manager.handle_pause_button(260, true);
    TEST_ASSERT_TRUE(toggled_resume);
    TEST_ASSERT_FALSE(manager.is_paused());
    TEST_ASSERT_EQUAL_UINT8(50, manager.get_motor_duty());
    TEST_ASSERT_EQUAL_UINT8(MOTOR_MEDIUM, state.get_motor_state());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_hbridge_motor_mock);
    RUN_TEST(test_servo_gate_driver_mock);
    RUN_TEST(test_debounced_button_timing_and_noise);
    RUN_TEST(test_actuation_manager_normal_operation);
    RUN_TEST(test_actuation_manager_motor_command);
    RUN_TEST(test_machine_pause_hardware_lockout);
    RUN_TEST(test_pause_preserves_led_binary_counts);
    RUN_TEST(test_telemetry_packet_generation_on_actuation_transitions);
    return UNITY_END();
}

#include <unity.h>
#include "counting_manager.h"
#include "actuator_state.h"
#include "led_bank_driver.h"
#include "protocol.h"

void setUp(void) {}
void tearDown(void) {}

void test_initial_state(void) {
    ActuatorState state;
    MockLedBankDriver led_driver;
    CountingManager cm(state, led_driver, 800);
    cm.begin();

    TEST_ASSERT_TRUE(led_driver.was_begin_called());
    TEST_ASSERT_EQUAL_UINT8(0, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0, cm.get_count(SHAPE_TRIANGLE));
    TEST_ASSERT_EQUAL_UINT8(0, cm.get_count(SHAPE_SQUARE));

    TEST_ASSERT_EQUAL_UINT8(0, cm.get_bank_pattern(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0, cm.get_bank_pattern(SHAPE_TRIANGLE));
    TEST_ASSERT_EQUAL_UINT8(0, cm.get_bank_pattern(SHAPE_SQUARE));

    TEST_ASSERT_FALSE(cm.is_in_observation_delay(SHAPE_CIRCLE));
    TEST_ASSERT_FALSE(cm.is_in_observation_delay(SHAPE_TRIANGLE));
    TEST_ASSERT_FALSE(cm.is_in_observation_delay(SHAPE_SQUARE));
}

void test_binary_led_patterns_and_increments(void) {
    ActuatorState state;
    MockLedBankDriver led_driver;
    CountingManager cm(state, led_driver, 800);
    cm.begin();

    // Verify Circle / Red Bank
    // Step 1: 0 -> 1 (binary 001)
    TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_CIRCLE, 100));
    TEST_ASSERT_EQUAL_UINT8(1, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0b001, cm.get_bank_pattern(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(1, state.get_red_count());

    // Step 2: 1 -> 2 (binary 010)
    TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_CIRCLE, 200));
    TEST_ASSERT_EQUAL_UINT8(2, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0b010, cm.get_bank_pattern(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(2, state.get_red_count());

    // Step 3: 2 -> 3 (binary 011)
    TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_CIRCLE, 300));
    TEST_ASSERT_EQUAL_UINT8(3, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0b011, cm.get_bank_pattern(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(3, state.get_red_count());

    // Step 4: 3 -> 4 (binary 100)
    TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_CIRCLE, 400));
    TEST_ASSERT_EQUAL_UINT8(4, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0b100, cm.get_bank_pattern(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(4, state.get_red_count());

    // Step 5: 4 -> 5 (binary 101)
    TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_CIRCLE, 500));
    TEST_ASSERT_EQUAL_UINT8(5, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0b101, cm.get_bank_pattern(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(5, state.get_red_count());
}

void test_observation_delay_engagement_and_duplicate_dropping(void) {
    ActuatorState state;
    MockLedBankDriver led_driver;
    CountingManager cm(state, led_driver, 800);
    cm.begin();

    // Advance Triangle (Green) to 5 at t=1000
    for (uint32_t t = 100; t <= 500; t += 100) {
        TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_TRIANGLE, t));
    }
    TEST_ASSERT_EQUAL_UINT8(5, cm.get_count(SHAPE_TRIANGLE));
    TEST_ASSERT_EQUAL_UINT8(0b101, cm.get_bank_pattern(SHAPE_TRIANGLE));
    TEST_ASSERT_TRUE(cm.is_in_observation_delay(SHAPE_TRIANGLE));

    // At t=600 (100ms into delay), tick should NOT expire
    BatchRolloverPayload rollover = {};
    TEST_ASSERT_FALSE(cm.tick(600, &rollover));

    // Duplicate detection for Triangle arriving at t=700 MUST be dropped
    TEST_ASSERT_FALSE(cm.handle_shape_detection(SHAPE_TRIANGLE, 700));
    TEST_ASSERT_EQUAL_UINT8(5, cm.get_count(SHAPE_TRIANGLE));
    TEST_ASSERT_EQUAL_UINT8(0b101, cm.get_bank_pattern(SHAPE_TRIANGLE));

    // At t=1299 (799ms into delay), tick still does NOT expire
    TEST_ASSERT_FALSE(cm.tick(1299, &rollover));

    // Duplicate detection at t=1299 still dropped
    TEST_ASSERT_FALSE(cm.handle_shape_detection(SHAPE_TRIANGLE, 1299));
}

void test_other_shapes_unaffected_during_active_observation_delay(void) {
    ActuatorState state;
    MockLedBankDriver led_driver;
    CountingManager cm(state, led_driver, 800);
    cm.begin();

    // Bring Blue / Square to 5 at t=1000
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_SQUARE, 1000));
    }
    TEST_ASSERT_TRUE(cm.is_in_observation_delay(SHAPE_SQUARE));
    TEST_ASSERT_EQUAL_UINT8(0b101, cm.get_bank_pattern(SHAPE_SQUARE));

    // During Square's delay, Red / Circle detection MUST be accepted and increment normally
    TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_CIRCLE, 1200));
    TEST_ASSERT_EQUAL_UINT8(1, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0b001, cm.get_bank_pattern(SHAPE_CIRCLE));

    // And Triangle / Green detection MUST also increment normally
    TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_TRIANGLE, 1300));
    TEST_ASSERT_EQUAL_UINT8(1, cm.get_count(SHAPE_TRIANGLE));
    TEST_ASSERT_EQUAL_UINT8(0b001, cm.get_bank_pattern(SHAPE_TRIANGLE));

    // Square duplicate still dropped
    TEST_ASSERT_FALSE(cm.handle_shape_detection(SHAPE_SQUARE, 1350));
}

void test_observation_delay_expiration_and_batch_rollover(void) {
    ActuatorState state;
    MockLedBankDriver led_driver;
    CountingManager cm(state, led_driver, 800);
    cm.begin();

    // Red reaches 5 at t=2000
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_CIRCLE, 2000));
    }
    TEST_ASSERT_TRUE(cm.is_in_observation_delay(SHAPE_CIRCLE));

    // At t=2800 (exactly 800ms later), tick expires observation delay
    BatchRolloverPayload rollover = {};
    bool rolled = cm.tick(2800, &rollover);
    TEST_ASSERT_TRUE(rolled);
    TEST_ASSERT_EQUAL_UINT8(SHAPE_CIRCLE, rollover.shape_id);
    TEST_ASSERT_EQUAL_UINT8(5, rollover.batch_size);
    TEST_ASSERT_EQUAL_UINT32(2800, rollover.timestamp_ms);

    // After rollover: count resets to 0, LEDs turn off (000), observation delay flag clears
    TEST_ASSERT_EQUAL_UINT8(0, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0b000, cm.get_bank_pattern(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0, state.get_red_count());
    TEST_ASSERT_FALSE(cm.is_in_observation_delay(SHAPE_CIRCLE));

    // Subsequent tick produces no further rollover
    TEST_ASSERT_FALSE(cm.tick(2801, &rollover));

    // New Circle detection is accepted after rollover and starts cycle anew
    TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_CIRCLE, 2900));
    TEST_ASSERT_EQUAL_UINT8(1, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0b001, cm.get_bank_pattern(SHAPE_CIRCLE));
}

void test_counting_lockout_during_machine_pause(void) {
    ActuatorState state;
    MockLedBankDriver led_driver;
    CountingManager cm(state, led_driver, 800);
    cm.begin();

    // Normal detection
    TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_CIRCLE, 100));
    TEST_ASSERT_EQUAL_UINT8(1, cm.get_count(SHAPE_CIRCLE));

    // Machine Pause engaged
    state.set_paused(true);

    // Any shape detection MUST be dropped
    TEST_ASSERT_FALSE(cm.handle_shape_detection(SHAPE_CIRCLE, 200));
    TEST_ASSERT_FALSE(cm.handle_shape_detection(SHAPE_TRIANGLE, 200));
    TEST_ASSERT_FALSE(cm.handle_shape_detection(SHAPE_SQUARE, 200));

    // Count is preserved at 1
    TEST_ASSERT_EQUAL_UINT8(1, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0, cm.get_count(SHAPE_TRIANGLE));

    // Machine Pause cleared (resume)
    state.set_paused(false);

    // Detection accepted again
    TEST_ASSERT_TRUE(cm.handle_shape_detection(SHAPE_CIRCLE, 300));
    TEST_ASSERT_EQUAL_UINT8(2, cm.get_count(SHAPE_CIRCLE));
    TEST_ASSERT_EQUAL_UINT8(0b010, cm.get_bank_pattern(SHAPE_CIRCLE));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_initial_state);
    RUN_TEST(test_binary_led_patterns_and_increments);
    RUN_TEST(test_observation_delay_engagement_and_duplicate_dropping);
    RUN_TEST(test_other_shapes_unaffected_during_active_observation_delay);
    RUN_TEST(test_observation_delay_expiration_and_batch_rollover);
    RUN_TEST(test_counting_lockout_during_machine_pause);
    return UNITY_END();
}

#include <unity.h>
#include <string.h>
#include "auth_terminal.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_phase1_numeric_entry_and_backspace(void) {
    AuthTerminal terminal;
    TEST_ASSERT_EQUAL(STATE_ENTER_USER_ID, terminal.get_state());
    TEST_ASSERT_EQUAL_STRING("", terminal.get_user_id_str());

    // Enter digits 1, 0, 4, 2
    TEST_ASSERT_FALSE(terminal.handle_key('1'));
    TEST_ASSERT_FALSE(terminal.handle_key('0'));
    TEST_ASSERT_FALSE(terminal.handle_key('4'));
    TEST_ASSERT_FALSE(terminal.handle_key('2'));
    TEST_ASSERT_EQUAL_STRING("1042", terminal.get_user_id_str());
    TEST_ASSERT_EQUAL_UINT32(1042, terminal.get_user_id());

    // Press '*' to backspace
    TEST_ASSERT_FALSE(terminal.handle_key('*'));
    TEST_ASSERT_EQUAL_STRING("104", terminal.get_user_id_str());
    TEST_ASSERT_EQUAL_UINT32(104, terminal.get_user_id());

    // Non-numeric keys like 'A', 'B' should be ignored
    TEST_ASSERT_FALSE(terminal.handle_key('A'));
    TEST_ASSERT_FALSE(terminal.handle_key('B'));
    TEST_ASSERT_EQUAL_STRING("104", terminal.get_user_id_str());

    // Backspace to empty and extra backspace
    TEST_ASSERT_FALSE(terminal.handle_key('*'));
    TEST_ASSERT_FALSE(terminal.handle_key('*'));
    TEST_ASSERT_FALSE(terminal.handle_key('*'));
    TEST_ASSERT_EQUAL_STRING("", terminal.get_user_id_str());
    TEST_ASSERT_FALSE(terminal.handle_key('*'));
    TEST_ASSERT_EQUAL_STRING("", terminal.get_user_id_str());
}

void test_phase1_advance_to_phase2_on_hash(void) {
    AuthTerminal terminal;

    // Press '#' when empty: should NOT advance
    TEST_ASSERT_FALSE(terminal.handle_key('#'));
    TEST_ASSERT_EQUAL(STATE_ENTER_USER_ID, terminal.get_state());

    // Enter User ID "501"
    terminal.handle_key('5');
    terminal.handle_key('0');
    terminal.handle_key('1');
    TEST_ASSERT_EQUAL_STRING("501", terminal.get_user_id_str());

    // Press '#' when non-empty: should advance to Phase 2 (STATE_ENTER_PIN)
    TEST_ASSERT_FALSE(terminal.handle_key('#'));
    TEST_ASSERT_EQUAL(STATE_ENTER_PIN, terminal.get_state());
    TEST_ASSERT_EQUAL_STRING("", terminal.get_pin_str());
    // User ID is preserved
    TEST_ASSERT_EQUAL_STRING("501", terminal.get_user_id_str());
}

void test_phase2_pin_entry_and_backspace_cancellation(void) {
    AuthTerminal terminal;

    // Advance to Phase 2 with User ID "42"
    terminal.handle_key('4');
    terminal.handle_key('2');
    terminal.handle_key('#');
    TEST_ASSERT_EQUAL(STATE_ENTER_PIN, terminal.get_state());

    // Enter PIN "1234"
    TEST_ASSERT_FALSE(terminal.handle_key('1'));
    TEST_ASSERT_FALSE(terminal.handle_key('2'));
    TEST_ASSERT_FALSE(terminal.handle_key('3'));
    TEST_ASSERT_FALSE(terminal.handle_key('4'));
    TEST_ASSERT_EQUAL_STRING("1234", terminal.get_pin_str());

    // Backspace once -> "123"
    TEST_ASSERT_FALSE(terminal.handle_key('*'));
    TEST_ASSERT_EQUAL_STRING("123", terminal.get_pin_str());

    // Backspace remaining digits
    terminal.handle_key('*');
    terminal.handle_key('*');
    terminal.handle_key('*');
    TEST_ASSERT_EQUAL_STRING("", terminal.get_pin_str());
    TEST_ASSERT_EQUAL(STATE_ENTER_PIN, terminal.get_state());

    // Press '*' when PIN is empty -> should cancel back to Phase 1 (STATE_ENTER_USER_ID)
    TEST_ASSERT_FALSE(terminal.handle_key('*'));
    TEST_ASSERT_EQUAL(STATE_ENTER_USER_ID, terminal.get_state());
    // User ID is still intact
    TEST_ASSERT_EQUAL_STRING("42", terminal.get_user_id_str());
}

void test_phase2_submission_and_json_formatting(void) {
    AuthTerminal terminal;

    // Advance to Phase 2 with User ID "1042"
    terminal.handle_key('1');
    terminal.handle_key('0');
    terminal.handle_key('4');
    terminal.handle_key('2');
    terminal.handle_key('#');
    TEST_ASSERT_EQUAL(STATE_ENTER_PIN, terminal.get_state());

    // Press '#' with empty PIN -> should not submit
    TEST_ASSERT_FALSE(terminal.handle_key('#'));
    TEST_ASSERT_EQUAL(STATE_ENTER_PIN, terminal.get_state());

    // Enter PIN "9876"
    terminal.handle_key('9');
    terminal.handle_key('8');
    terminal.handle_key('7');
    terminal.handle_key('6');

    // Submit with '#'
    bool submitted = terminal.handle_key('#');
    TEST_ASSERT_TRUE(submitted);
    TEST_ASSERT_EQUAL(STATE_AUTHENTICATING, terminal.get_state());

    // Check JSON payload formatting
    char json_buf[64] = {0};
    bool json_ok = terminal.format_auth_request_json(json_buf, sizeof(json_buf));
    TEST_ASSERT_TRUE(json_ok);
    TEST_ASSERT_EQUAL_STRING("{\"user_id\": 1042, \"pin\": \"9876\"}", json_buf);

    // Check AuthRequest struct
    AuthRequest req = {};
    TEST_ASSERT_TRUE(terminal.get_auth_request(&req));
    TEST_ASSERT_EQUAL_UINT32(1042, req.user_id);
    TEST_ASSERT_EQUAL_STRING("9876", req.pin);
}

void test_auth_submission_blocked_when_offline(void) {
    AuthTerminal terminal;
    terminal.set_network_connected(false);
    TEST_ASSERT_FALSE(terminal.is_network_connected());

    terminal.handle_key('1');
    terminal.handle_key('#');
    terminal.handle_key('9');

    // Submission on '#' must be blocked
    bool submitted = terminal.handle_key('#');
    TEST_ASSERT_FALSE(submitted);
    TEST_ASSERT_EQUAL(STATE_OFFLINE_BLOCKED, terminal.get_state());
}

void test_auth_response_success_and_3s_duration(void) {
    AuthTerminal terminal;
    terminal.handle_key('1');
    terminal.handle_key('#');
    terminal.handle_key('9');
    terminal.handle_key('#');
    TEST_ASSERT_EQUAL(STATE_AUTHENTICATING, terminal.get_state());

    AuthResponse resp = {};
    resp.status = AUTH_STATUS_OK;
    strncpy(resp.username, "Alice", sizeof(resp.username) - 1);

    terminal.handle_auth_response(resp);
    TEST_ASSERT_EQUAL(STATE_AUTH_SUCCESS, terminal.get_state());
    TEST_ASSERT_EQUAL_STRING("Alice", terminal.get_username());

    // Advance 1500 ms -> should still be in success screen
    terminal.tick(1500);
    TEST_ASSERT_EQUAL(STATE_AUTH_SUCCESS, terminal.get_state());

    // Advance another 1500 ms (total 3000 ms) -> should return to idle User ID entry
    terminal.tick(1500);
    TEST_ASSERT_EQUAL(STATE_ENTER_USER_ID, terminal.get_state());
    TEST_ASSERT_EQUAL_STRING("", terminal.get_user_id_str());
    TEST_ASSERT_EQUAL_STRING("", terminal.get_pin_str());
}

void test_auth_response_invalid_pin_and_attempts(void) {
    AuthTerminal terminal;
    terminal.handle_key('1');
    terminal.handle_key('#');
    terminal.handle_key('9');
    terminal.handle_key('#');

    AuthResponse resp = {};
    resp.status = AUTH_STATUS_INVALID_PIN;
    resp.remaining_attempts = 2;

    terminal.handle_auth_response(resp);
    TEST_ASSERT_EQUAL(STATE_AUTH_INVALID_PIN, terminal.get_state());
    TEST_ASSERT_EQUAL_UINT8(2, terminal.get_remaining_attempts());

    // Advance past display timeout -> returns to PIN entry to retry
    terminal.tick(3000);
    TEST_ASSERT_EQUAL(STATE_ENTER_PIN, terminal.get_state());
    // PIN buffer should be cleared for re-entry, User ID preserved
    TEST_ASSERT_EQUAL_STRING("1", terminal.get_user_id_str());
    TEST_ASSERT_EQUAL_STRING("", terminal.get_pin_str());
}

void test_auth_response_user_locked_countdown(void) {
    AuthTerminal terminal;
    terminal.handle_key('1');
    terminal.handle_key('#');
    terminal.handle_key('9');
    terminal.handle_key('#');

    AuthResponse resp = {};
    resp.status = AUTH_STATUS_USER_LOCKED;
    resp.lockout_seconds = 3;

    terminal.handle_auth_response(resp);
    TEST_ASSERT_EQUAL(STATE_AUTH_LOCKED, terminal.get_state());
    TEST_ASSERT_EQUAL_UINT32(3, terminal.get_lockout_remaining_seconds());

    // After 1 second
    terminal.tick(1000);
    TEST_ASSERT_EQUAL(STATE_AUTH_LOCKED, terminal.get_state());
    TEST_ASSERT_EQUAL_UINT32(2, terminal.get_lockout_remaining_seconds());

    // After another 1 second
    terminal.tick(1000);
    TEST_ASSERT_EQUAL(STATE_AUTH_LOCKED, terminal.get_state());
    TEST_ASSERT_EQUAL_UINT32(1, terminal.get_lockout_remaining_seconds());

    // After final 1 second (lockout expired)
    terminal.tick(1000);
    TEST_ASSERT_EQUAL(STATE_ENTER_USER_ID, terminal.get_state());
    TEST_ASSERT_EQUAL_STRING("", terminal.get_user_id_str());
}

void test_auth_response_user_not_found(void) {
    AuthTerminal terminal;
    terminal.handle_key('9');
    terminal.handle_key('#');
    terminal.handle_key('9');
    terminal.handle_key('#');

    AuthResponse resp = {};
    resp.status = AUTH_STATUS_USER_NOT_FOUND;

    terminal.handle_auth_response(resp);
    TEST_ASSERT_EQUAL(STATE_AUTH_NOT_FOUND, terminal.get_state());

    // After display timeout -> returns to idle Phase 1
    terminal.tick(3000);
    TEST_ASSERT_EQUAL(STATE_ENTER_USER_ID, terminal.get_state());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_phase1_numeric_entry_and_backspace);
    RUN_TEST(test_phase1_advance_to_phase2_on_hash);
    RUN_TEST(test_phase2_pin_entry_and_backspace_cancellation);
    RUN_TEST(test_phase2_submission_and_json_formatting);
    RUN_TEST(test_auth_submission_blocked_when_offline);
    RUN_TEST(test_auth_response_success_and_3s_duration);
    RUN_TEST(test_auth_response_invalid_pin_and_attempts);
    RUN_TEST(test_auth_response_user_locked_countdown);
    RUN_TEST(test_auth_response_user_not_found);
    return UNITY_END();
}





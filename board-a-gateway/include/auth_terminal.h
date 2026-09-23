#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum TerminalState {
    STATE_ENTER_USER_ID,
    STATE_ENTER_PIN,
    STATE_AUTHENTICATING,
    STATE_AUTH_SUCCESS,
    STATE_AUTH_INVALID_PIN,
    STATE_AUTH_LOCKED,
    STATE_AUTH_NOT_FOUND,
    STATE_OFFLINE_BLOCKED
};

enum AuthStatus {
    AUTH_STATUS_OK,
    AUTH_STATUS_INVALID_PIN,
    AUTH_STATUS_USER_LOCKED,
    AUTH_STATUS_USER_NOT_FOUND,
    AUTH_STATUS_UNKNOWN
};

struct AuthRequest {
    uint32_t user_id;
    char pin[16];
};

struct AuthResponse {
    AuthStatus status;
    char username[32];
    uint8_t remaining_attempts;
    uint32_t lockout_seconds;
};

class AuthTerminal {
public:
    static const size_t MAX_ID_LEN = 10;
    static const size_t MAX_PIN_LEN = 8;
    static const uint32_t SUCCESS_DISPLAY_MS = 3000;
    static const uint32_t MESSAGE_DISPLAY_MS = 3000;

    AuthTerminal();

    void reset();
    void set_network_connected(bool connected);
    bool is_network_connected() const;

    TerminalState get_state() const;
    const char* get_user_id_str() const;
    uint32_t get_user_id() const;
    const char* get_pin_str() const;
    const char* get_username() const;
    uint8_t get_remaining_attempts() const;
    uint32_t get_lockout_remaining_seconds() const;

    // Handles single keypress: '0'-'9', '*', '#'
    // Returns true if a submission request was triggered
    bool handle_key(char key);

    // Processes incoming server auth response
    void handle_auth_response(const AuthResponse& resp);

    // Timer advancement in milliseconds
    void tick(uint32_t delta_ms);

    // Formats JSON request string matching {"user_id": <int>, "pin": "<str>"}
    bool format_auth_request_json(char* buffer, size_t max_len) const;

    // Fills out AuthRequest struct for queue dispatch
    bool get_auth_request(AuthRequest* req) const;

private:
    TerminalState state_;
    bool network_connected_;
    char user_id_buf_[MAX_ID_LEN + 1];
    size_t user_id_len_;
    char pin_buf_[MAX_PIN_LEN + 1];
    size_t pin_len_;

    char username_[32];
    uint8_t remaining_attempts_;
    uint32_t lockout_remaining_seconds_;
    uint32_t timer_ms_;
    uint32_t countdown_accumulator_ms_;
};

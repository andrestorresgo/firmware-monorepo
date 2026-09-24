#include "auth_terminal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

AuthTerminal::AuthTerminal() {
    reset();
}

void AuthTerminal::reset() {
    state_ = STATE_ENTER_USER_ID;
    network_connected_ = true;
    user_id_buf_[0] = '\0';
    user_id_len_ = 0;
    pin_buf_[0] = '\0';
    pin_len_ = 0;
    username_[0] = '\0';
    remaining_attempts_ = 0;
    lockout_remaining_seconds_ = 0;
    timer_ms_ = 0;
    countdown_accumulator_ms_ = 0;
}

void AuthTerminal::set_network_connected(bool connected) {
    network_connected_ = connected;
}

bool AuthTerminal::is_network_connected() const {
    return network_connected_;
}

TerminalState AuthTerminal::get_state() const {
    return state_;
}

const char* AuthTerminal::get_user_id_str() const {
    return user_id_buf_;
}

uint32_t AuthTerminal::get_user_id() const {
    return (uint32_t)strtoul(user_id_buf_, NULL, 10);
}

const char* AuthTerminal::get_pin_str() const {
    return pin_buf_;
}

const char* AuthTerminal::get_username() const {
    return username_;
}

uint8_t AuthTerminal::get_remaining_attempts() const {
    return remaining_attempts_;
}

uint32_t AuthTerminal::get_lockout_remaining_seconds() const {
    return lockout_remaining_seconds_;
}

bool AuthTerminal::handle_key(char key) {
    if (state_ == STATE_ENTER_USER_ID) {
        if (key >= '0' && key <= '9') {
            if (user_id_len_ < MAX_ID_LEN) {
                user_id_buf_[user_id_len_++] = key;
                user_id_buf_[user_id_len_] = '\0';
            }
            return false;
        } else if (key == '*') {
            if (user_id_len_ > 0) {
                user_id_buf_[--user_id_len_] = '\0';
            }
            return false;
        } else if (key == '#') {
            if (user_id_len_ > 0) {
                state_ = STATE_ENTER_PIN;
                pin_buf_[0] = '\0';
                pin_len_ = 0;
            }
            return false;
        }

    } else if (state_ == STATE_ENTER_PIN) {
        if (key >= '0' && key <= '9') {
            if (pin_len_ < MAX_PIN_LEN) {
                pin_buf_[pin_len_++] = key;
                pin_buf_[pin_len_] = '\0';
            }
            return false;
        } else if (key == '*') {
            if (pin_len_ > 0) {
                pin_buf_[--pin_len_] = '\0';
            } else {
                // Buffer is empty: cancel back to Phase 1
                state_ = STATE_ENTER_USER_ID;
            }
            return false;
        } else if (key == '#') {
            if (pin_len_ == 0) {
                return false;
            }
            if (!network_connected_) {
                state_ = STATE_OFFLINE_BLOCKED;
                timer_ms_ = 0;
                return false;
            }
            state_ = STATE_AUTHENTICATING;
            return true;
        }
    } else if (state_ == STATE_AUTHENTICATING) {
        if (key == '*') {
            // Cancel back to PIN entry
            state_ = STATE_ENTER_PIN;
            timer_ms_ = 0;
            return false;
        }
    } else if (state_ == STATE_OFFLINE_BLOCKED) {
        // Any key returns to PIN entry to retry or edit
        state_ = STATE_ENTER_PIN;
        return false;
    }
    return false;
}


void AuthTerminal::handle_auth_response(const AuthResponse& resp) {
    if (state_ != STATE_AUTHENTICATING) return;

    timer_ms_ = 0;
    countdown_accumulator_ms_ = 0;

    switch (resp.status) {
        case AUTH_STATUS_OK:
            state_ = STATE_AUTH_SUCCESS;
            strncpy(username_, resp.username, sizeof(username_) - 1);
            username_[sizeof(username_) - 1] = '\0';
            break;
        case AUTH_STATUS_INVALID_PIN:
            state_ = STATE_AUTH_INVALID_PIN;
            remaining_attempts_ = resp.remaining_attempts;
            break;
        case AUTH_STATUS_USER_LOCKED:
            state_ = STATE_AUTH_LOCKED;
            lockout_remaining_seconds_ = resp.lockout_seconds;
            break;
        case AUTH_STATUS_USER_NOT_FOUND:
            state_ = STATE_AUTH_NOT_FOUND;
            break;
        default:
            state_ = STATE_ENTER_USER_ID;
            break;
    }
}

void AuthTerminal::tick(uint32_t delta_ms) {
    timer_ms_ += delta_ms;

    if (state_ == STATE_AUTHENTICATING) {
        if (timer_ms_ >= AUTHENTICATING_TIMEOUT_MS) {
            state_ = STATE_OFFLINE_BLOCKED;
            timer_ms_ = 0;
        }
    } else if (state_ == STATE_AUTH_SUCCESS) {
        if (timer_ms_ >= SUCCESS_DISPLAY_MS) {
            reset();
        }
    } else if (state_ == STATE_AUTH_INVALID_PIN) {
        if (timer_ms_ >= MESSAGE_DISPLAY_MS) {
            state_ = STATE_ENTER_PIN;
            pin_buf_[0] = '\0';
            pin_len_ = 0;
            timer_ms_ = 0;
        }
    } else if (state_ == STATE_AUTH_LOCKED) {
        countdown_accumulator_ms_ += delta_ms;
        while (countdown_accumulator_ms_ >= 1000) {
            countdown_accumulator_ms_ -= 1000;
            if (lockout_remaining_seconds_ > 0) {
                lockout_remaining_seconds_--;
            }
        }
        if (lockout_remaining_seconds_ == 0) {
            reset();
        }
    } else if (state_ == STATE_AUTH_NOT_FOUND) {
        if (timer_ms_ >= MESSAGE_DISPLAY_MS) {
            reset();
        }
    }
}


bool AuthTerminal::format_auth_request_json(char* buffer, size_t max_len) const {
    if (!buffer || max_len == 0) return false;
    int written = snprintf(buffer, max_len, "{\"user_id\": %u, \"pin\": \"%s\"}", get_user_id(), pin_buf_);
    return (written > 0 && (size_t)written < max_len);
}

bool AuthTerminal::get_auth_request(AuthRequest* req) const {
    if (!req) return false;
    req->user_id = get_user_id();
    strncpy(req->pin, pin_buf_, sizeof(req->pin) - 1);
    req->pin[sizeof(req->pin) - 1] = '\0';
    return true;
}

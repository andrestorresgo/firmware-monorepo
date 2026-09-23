#ifndef UNIT_TEST

#include "terminal_display.h"
#include <stdio.h>

TerminalDisplay::TerminalDisplay()
    : u8g2_(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 22, /* data=*/ 21) {
}

void TerminalDisplay::begin() {
    u8g2_.begin();
    u8g2_.clearBuffer();
    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(20, 36, "Terminal Init...");
    u8g2_.sendBuffer();
}

void TerminalDisplay::render(const AuthTerminal& terminal) {
    u8g2_.clearBuffer();

    render_header(terminal.is_network_connected());

    switch (terminal.get_state()) {
        case STATE_ENTER_USER_ID:
            render_user_id_entry(terminal);
            break;
        case STATE_ENTER_PIN:
            render_pin_entry(terminal);
            break;
        case STATE_AUTHENTICATING:
            render_authenticating();
            break;
        case STATE_AUTH_SUCCESS:
            render_auth_success(terminal);
            break;
        case STATE_AUTH_INVALID_PIN:
            render_invalid_pin(terminal);
            break;
        case STATE_AUTH_LOCKED:
            render_user_locked(terminal);
            break;
        case STATE_AUTH_NOT_FOUND:
            render_user_not_found();
            break;
        case STATE_OFFLINE_BLOCKED:
            render_offline_blocked();
            break;
    }

    u8g2_.sendBuffer();
}

void TerminalDisplay::render_header(bool online) {
    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(0, 9, "ACCESS TERMINAL");
    
    if (online) {
        u8g2_.drawStr(90, 9, "[ONLINE]");
    } else {
        u8g2_.drawStr(84, 9, "[OFFLINE]");
    }
    
    u8g2_.drawHLine(0, 12, 128);
}

void TerminalDisplay::render_user_id_entry(const AuthTerminal& terminal) {
    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(0, 24, "Enter User ID:");

    char display_buf[32];
    snprintf(display_buf, sizeof(display_buf), "> %s_", terminal.get_user_id_str());
    u8g2_.setFont(u8g2_font_7x14B_tf);
    u8g2_.drawStr(4, 40, display_buf);

    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawHLine(0, 50, 128);
    u8g2_.drawStr(0, 62, "#:Next       *:Del");
}

void TerminalDisplay::render_pin_entry(const AuthTerminal& terminal) {
    u8g2_.setFont(u8g2_font_6x10_tf);
    char user_str[32];
    snprintf(user_str, sizeof(user_str), "User ID: %s", terminal.get_user_id_str());
    u8g2_.drawStr(0, 23, user_str);

    u8g2_.drawStr(0, 34, "Enter PIN (Plain):");

    char pin_buf[32];
    snprintf(pin_buf, sizeof(pin_buf), "> %s_", terminal.get_pin_str());
    u8g2_.setFont(u8g2_font_7x14B_tf);
    u8g2_.drawStr(4, 48, pin_buf);

    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawHLine(0, 52, 128);
    u8g2_.drawStr(0, 63, "#:Submit     *:Back");
}

void TerminalDisplay::render_authenticating() {
    u8g2_.setFont(u8g2_font_7x14B_tf);
    u8g2_.drawStr(12, 32, "Authenticating");
    
    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(16, 46, "Contacting Cloud...");
    u8g2_.drawStr(28, 60, "Please wait");
}

void TerminalDisplay::render_auth_success(const AuthTerminal& terminal) {
    u8g2_.setFont(u8g2_font_7x14B_tf);
    u8g2_.drawStr(10, 28, "ACCESS GRANTED");

    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(10, 42, "Welcome,");
    
    u8g2_.setFont(u8g2_font_7x14B_tf);
    u8g2_.drawStr(10, 58, terminal.get_username());
}

void TerminalDisplay::render_invalid_pin(const AuthTerminal& terminal) {
    u8g2_.setFont(u8g2_font_7x14B_tf);
    u8g2_.drawStr(14, 28, "ACCESS DENIED");

    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(24, 42, "Invalid PIN!");

    char attempt_buf[32];
    snprintf(attempt_buf, sizeof(attempt_buf), "Attempts left: %u", terminal.get_remaining_attempts());
    u8g2_.drawStr(14, 56, attempt_buf);
}

void TerminalDisplay::render_user_locked(const AuthTerminal& terminal) {
    u8g2_.setFont(u8g2_font_7x14B_tf);
    u8g2_.drawStr(16, 28, "USER LOCKED");

    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(8, 42, "Exceeded Max Tries");

    char timer_buf[32];
    snprintf(timer_buf, sizeof(timer_buf), "Retry in: %us", terminal.get_lockout_remaining_seconds());
    u8g2_.drawStr(22, 56, timer_buf);
}

void TerminalDisplay::render_user_not_found() {
    u8g2_.setFont(u8g2_font_7x14B_tf);
    u8g2_.drawStr(14, 28, "ACCESS DENIED");

    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(10, 44, "User ID Not Found");
    u8g2_.drawStr(14, 58, "Check ID and retry");
}

void TerminalDisplay::render_offline_blocked() {
    u8g2_.setFont(u8g2_font_7x14B_tf);
    u8g2_.drawStr(14, 28, "CLOUD OFFLINE");

    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(8, 42, "Submission Blocked");
    u8g2_.drawStr(8, 58, "Press any key to back");
}

#endif // !UNIT_TEST

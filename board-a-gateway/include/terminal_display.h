#pragma once

#ifndef UNIT_TEST

#include <U8g2lib.h>
#include "auth_terminal.h"

class TerminalDisplay {
public:
    TerminalDisplay();

    // Initializes I2C OLED display with correct SH1106 parameters on GPIO 21 (SDA) / GPIO 22 (SCL)
    void begin();

    // Renders the current terminal state to the OLED screen
    void render(const AuthTerminal& terminal);

private:
    U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2_;

    void render_header(bool online);
    void render_user_id_entry(const AuthTerminal& terminal);
    void render_pin_entry(const AuthTerminal& terminal);
    void render_authenticating();
    void render_auth_success(const AuthTerminal& terminal);
    void render_invalid_pin(const AuthTerminal& terminal);
    void render_user_locked(const AuthTerminal& terminal);
    void render_user_not_found();
    void render_offline_blocked();
};

#endif // !UNIT_TEST

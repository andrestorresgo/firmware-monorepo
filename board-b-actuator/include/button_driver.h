#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PIN_PAUSE_BUTTON 25

// Software-debounced momentary button handler
class DebouncedButton {
public:
    explicit DebouncedButton(uint32_t debounce_ms = 50)
        : debounce_ms_(debounce_ms),
          last_raw_reading_(false),
          debounced_state_(false),
          last_state_change_ms_(0) {}

    // Updates button state with raw pressed reading (true if physically pressed).
    // Returns true once on transition to pressed after stable debounce duration.
    bool update(uint32_t now_ms, bool raw_pressed) {
        if (raw_pressed != last_raw_reading_) {
            last_raw_reading_ = raw_pressed;
            last_state_change_ms_ = now_ms;
        }

        if ((now_ms - last_state_change_ms_) >= debounce_ms_) {
            if (raw_pressed != debounced_state_) {
                debounced_state_ = raw_pressed;
                if (debounced_state_) {
                    return true; // Debounced transition to PRESSED (rising edge of pressed)
                }
            }
        }
        return false;
    }

    bool is_pressed() const {
        return debounced_state_;
    }

    void reset() {
        last_raw_reading_ = false;
        debounced_state_ = false;
        last_state_change_ms_ = 0;
    }

    uint32_t get_debounce_ms() const {
        return debounce_ms_;
    }

private:
    uint32_t debounce_ms_;
    bool last_raw_reading_;
    bool debounced_state_;
    uint32_t last_state_change_ms_;
};

#include "counting_manager.h"
#include <string.h>

CountingManager::CountingManager(ActuatorState& state,
                                 ILedBankDriver& led_driver,
                                 uint32_t observation_delay_ms)
    : state_(state),
      led_driver_(led_driver),
      observation_delay_ms_(observation_delay_ms) {
    memset(counts_, 0, sizeof(counts_));
    memset(in_observation_delay_, 0, sizeof(in_observation_delay_));
    memset(delay_start_ms_, 0, sizeof(delay_start_ms_));
}

void CountingManager::begin() {
    led_driver_.begin();
    reset();
}

void CountingManager::reset() {
    for (int i = 0; i < 3; ++i) {
        counts_[i] = 0;
        in_observation_delay_[i] = false;
        delay_start_ms_[i] = 0;
        uint8_t shape_id = index_to_shape(i);
        led_driver_.write_bank(shape_id, 0);
        state_.set_shape_count(shape_id, 0);
    }
}

int CountingManager::shape_to_index(uint8_t shape_id) {
    switch (shape_id) {
        case SHAPE_CIRCLE:   return 0; // Red
        case SHAPE_TRIANGLE: return 1; // Green
        case SHAPE_SQUARE:   return 2; // Blue
        default:             return -1;
    }
}

uint8_t CountingManager::index_to_shape(int idx) {
    switch (idx) {
        case 0:  return SHAPE_CIRCLE;
        case 1:  return SHAPE_TRIANGLE;
        case 2:  return SHAPE_SQUARE;
        default: return SHAPE_UNKNOWN;
    }
}

bool CountingManager::handle_shape_detection(uint8_t shape_id, uint32_t now_ms) {
    // 1. Rejection during Machine Pause (ADR-0004)
    if (state_.is_paused()) {
        return false;
    }

    // 2. Validate Shape ID
    int idx = shape_to_index(shape_id);
    if (idx < 0) {
        return false;
    }

    // 3. Drop duplicate detections during active 800ms Observation Delay
    if (in_observation_delay_[idx]) {
        return false;
    }

    // 4. Increment count (0 -> 1 -> 2 -> 3 -> 4 -> 5)
    counts_[idx]++;
    if (counts_[idx] > 5) {
        counts_[idx] = 5;
    }

    // 5. Update Actuator State & LED Bank Pattern
    state_.set_shape_count(shape_id, counts_[idx]);
    led_driver_.write_bank(shape_id, counts_[idx]);

    // 6. If reached 5 (binary 101), engage 800ms Observation Delay hold
    if (counts_[idx] == 5) {
        in_observation_delay_[idx] = true;
        delay_start_ms_[idx] = now_ms;
    }

    return true;
}

bool CountingManager::tick(uint32_t now_ms, struct BatchRolloverPayload* out_rollover) {
    for (int idx = 0; idx < 3; ++idx) {
        if (in_observation_delay_[idx]) {
            if (now_ms - delay_start_ms_[idx] >= observation_delay_ms_) {
                // Observation delay expired:
                in_observation_delay_[idx] = false;
                counts_[idx] = 0;
                uint8_t shape_id = index_to_shape(idx);

                // Reset LED bank to 000 (all off)
                led_driver_.write_bank(shape_id, 0);

                // Reset Actuator State
                state_.set_shape_count(shape_id, 0);

                // Build Batch Rollover payload
                if (out_rollover) {
                    out_rollover->shape_id = shape_id;
                    out_rollover->batch_size = 5;
                    out_rollover->timestamp_ms = now_ms;
                }

                return true;
            }
        }
    }
    return false;
}

uint8_t CountingManager::get_count(uint8_t shape_id) const {
    int idx = shape_to_index(shape_id);
    if (idx < 0) return 0;
    return counts_[idx];
}

bool CountingManager::is_in_observation_delay(uint8_t shape_id) const {
    int idx = shape_to_index(shape_id);
    if (idx < 0) return false;
    return in_observation_delay_[idx];
}

uint8_t CountingManager::get_bank_pattern(uint8_t shape_id) const {
    return led_driver_.get_bank_pattern(shape_id);
}

uint32_t CountingManager::get_observation_delay_ms() const {
    return observation_delay_ms_;
}

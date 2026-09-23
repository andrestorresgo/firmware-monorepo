#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"
#include "actuator_state.h"
#include "led_bank_driver.h"

// Default observation delay hold period before batch rollover (Ticket 05)
#define DEFAULT_OBSERVATION_DELAY_MS 800

class CountingManager {
public:
    explicit CountingManager(ActuatorState& state,
                            ILedBankDriver& led_driver,
                            uint32_t observation_delay_ms = DEFAULT_OBSERVATION_DELAY_MS);

    // Initializes LED outputs to 0 (all LEDs off)
    void begin();

    // Ingests a shape detection event (SHAPE_CIRCLE, SHAPE_TRIANGLE, or SHAPE_SQUARE).
    // Increments the respective shape counter and updates LED bank in binary.
    // When count reaches 5 (101), enters the 800ms Observation Delay.
    // Duplicate detections for a shape in Observation Delay are dropped.
    // Any detections received while ActuatorState is paused are dropped.
    // Returns true if detection was accepted and processed, false if dropped.
    bool handle_shape_detection(uint8_t shape_id, uint32_t now_ms);

    // Advances timers. If an active Observation Delay expires (>= 800ms):
    // Resets counter and LEDs to 000, and populates out_rollover.
    // Returns true if a rollover occurred on this tick.
    bool tick(uint32_t now_ms, struct BatchRolloverPayload* out_rollover);

    // Queries
    uint8_t get_count(uint8_t shape_id) const;
    bool is_in_observation_delay(uint8_t shape_id) const;
    uint8_t get_bank_pattern(uint8_t shape_id) const;
    uint32_t get_observation_delay_ms() const;

    // Resets counters and delays
    void reset();

private:
    ActuatorState& state_;
    ILedBankDriver& led_driver_;
    uint32_t observation_delay_ms_;

    // Per-shape state (indices 0=Circle, 1=Triangle, 2=Square)
    uint8_t counts_[3];
    bool in_observation_delay_[3];
    uint32_t delay_start_ms_[3];

    static int shape_to_index(uint8_t shape_id);
    static uint8_t index_to_shape(int idx);
};

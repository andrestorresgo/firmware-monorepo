#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

class ActuatorState {
public:
    ActuatorState();

    bool is_paused() const;
    uint8_t get_motor_state() const;
    uint8_t get_servo_state() const;
    uint8_t get_red_count() const;
    uint8_t get_green_count() const;
    uint8_t get_blue_count() const;

    bool is_dirty() const;
    void clear_dirty();

    // Machine Pause controls (enforces hardware lockout per ADR-0004)
    void set_paused(bool paused);

    // Peripheral controls (rejected if machine is paused)
    bool set_motor_state(uint8_t state);
    bool set_servo_state(uint8_t state);
    bool set_shape_count(uint8_t shape_id, uint8_t count);

    // Telemetry generation helpers
    void build_telemetry_payload(uint32_t uptime_ms, struct TelemetryPayload* out_payload) const;
    int build_telemetry_packet(uint32_t uptime_ms, struct EspNowPacket* out_packet) const;

private:
    bool is_paused_;
    uint8_t motor_state_;
    uint8_t pre_pause_motor_state_;
    uint8_t servo_state_;
    uint8_t red_count_;
    uint8_t green_count_;
    uint8_t blue_count_;
    bool dirty_;
};

#include "actuator_state.h"
#include <string.h>

ActuatorState::ActuatorState()
    : is_paused_(false),
      motor_state_(true), // Running by default in normal operation (User Story 22)
      servo_state_(SERVO_CLOSED),
      red_count_(0),
      green_count_(0),
      blue_count_(0),
      dirty_(false) {}

bool ActuatorState::is_paused() const {
    return is_paused_;
}

bool ActuatorState::get_motor_state() const {
    return motor_state_;
}

uint8_t ActuatorState::get_servo_state() const {
    return servo_state_;
}

uint8_t ActuatorState::get_red_count() const {
    return red_count_;
}

uint8_t ActuatorState::get_green_count() const {
    return green_count_;
}

uint8_t ActuatorState::get_blue_count() const {
    return blue_count_;
}

bool ActuatorState::is_dirty() const {
    return dirty_;
}

void ActuatorState::clear_dirty() {
    dirty_ = false;
}

void ActuatorState::set_paused(bool paused) {
    if (is_paused_ != paused) {
        is_paused_ = paused;
        if (is_paused_) {
            // Cut power to DC conveyor motor upon pause (ADR-0004)
            motor_state_ = false;
        } else {
            // Restore DC conveyor motor upon resume (User Story 26)
            motor_state_ = true;
        }
        dirty_ = true;
    }
}

bool ActuatorState::set_motor_state(bool running) {
    if (is_paused_) {
        return false; // Hardware lockout
    }
    if (motor_state_ != running) {
        motor_state_ = running;
        dirty_ = true;
    }
    return true;
}

bool ActuatorState::set_servo_state(uint8_t state) {
    if (is_paused_) {
        return false; // Hardware lockout
    }
    if (servo_state_ != state) {
        servo_state_ = state;
        dirty_ = true;
    }
    return true;
}

bool ActuatorState::set_shape_count(uint8_t shape_id, uint8_t count) {
    if (is_paused_) {
        return false; // Counting blocked during pause
    }
    if (count > 5) {
        return false; // Counts are 0..5
    }

    switch (shape_id) {
        case SHAPE_CIRCLE:
            if (red_count_ != count) {
                red_count_ = count;
                dirty_ = true;
            }
            return true;
        case SHAPE_TRIANGLE:
            if (green_count_ != count) {
                green_count_ = count;
                dirty_ = true;
            }
            return true;
        case SHAPE_SQUARE:
            if (blue_count_ != count) {
                blue_count_ = count;
                dirty_ = true;
            }
            return true;
        default:
            return false;
    }
}

void ActuatorState::build_telemetry_payload(uint32_t uptime_ms, struct TelemetryPayload* out_payload) const {
    if (!out_payload) return;
    out_payload->is_paused = is_paused_ ? 1 : 0;
    out_payload->motor_state = motor_state_ ? 1 : 0;
    out_payload->servo_state = servo_state_;
    out_payload->red_count = red_count_;
    out_payload->green_count = green_count_;
    out_payload->blue_count = blue_count_;
    out_payload->uptime_ms = uptime_ms;
}

int ActuatorState::build_telemetry_packet(uint32_t uptime_ms, struct EspNowPacket* out_packet) const {
    if (!out_packet) return -1;
    memset(out_packet, 0, sizeof(struct EspNowPacket));
    out_packet->header.magic = ESPNOW_MAGIC_BYTE;
    out_packet->header.opcode = OPCODE_TELEMETRY;
    out_packet->header.payload_len = sizeof(struct TelemetryPayload);
    build_telemetry_payload(uptime_ms, &out_packet->payload.telemetry);
    return sizeof(struct FrameHeader) + sizeof(struct TelemetryPayload);
}

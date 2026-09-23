#include "actuation_manager.h"

ActuationManager::ActuationManager(ActuatorState& state,
                                   HBridgeMotor& motor,
                                   IServoGateDriver& servo,
                                   DebouncedButton& button)
    : state_(state),
      motor_(motor),
      servo_(servo),
      button_(button) {}

void ActuationManager::begin() {
    motor_.begin();
    servo_.begin();
    button_.reset();

    // In normal active operations, DC conveyor motor runs forward at 80% duty cycle
    motor_.drive_forward(80);
    state_.set_motor_state(true);

    // Initial servo gate is Closed (0 degrees)
    servo_.set_position(SERVO_CLOSED);
    state_.set_servo_state(SERVO_CLOSED);
}

bool ActuationManager::handle_pause_button(uint32_t now_ms, bool raw_pressed) {
    bool triggered = button_.update(now_ms, raw_pressed);
    if (!triggered) {
        return false;
    }

    // Toggle Machine Pause state
    bool new_paused = !state_.is_paused();
    state_.set_paused(new_paused);

    if (new_paused) {
        // Enforce total hardware lockout per ADR-0004: immediately cut DC motor PWM to 0%
        motor_.stop();
    } else {
        // Exiting Machine Pause restores DC motor power at 80% duty cycle
        motor_.drive_forward(80);
    }

    return true;
}

bool ActuationManager::handle_servo_command(uint8_t servo_state) {
    // 1. Lockout check: Reject and suppress remote servo commands during Machine Pause (ADR-0004)
    if (state_.is_paused()) {
        return false;
    }

    // 2. Validate position
    if (servo_state != SERVO_CLOSED && servo_state != SERVO_OPEN) {
        return false;
    }

    // 3. Actuate servo gate (0° Closed, 90° Open)
    servo_.set_position(servo_state);

    // 4. Update ActuatorState
    bool state_changed = (state_.get_servo_state() != servo_state);
    state_.set_servo_state(servo_state);

    return state_changed;
}

bool ActuationManager::is_paused() const {
    return state_.is_paused();
}

bool ActuationManager::is_motor_running() const {
    return motor_.is_running();
}

uint8_t ActuationManager::get_motor_duty() const {
    return motor_.get_duty_percent();
}

uint8_t ActuationManager::get_servo_state() const {
    return state_.get_servo_state();
}

int ActuationManager::get_servo_angle() const {
    return servo_.get_angle();
}

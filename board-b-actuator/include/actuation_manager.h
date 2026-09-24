#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"
#include "actuator_state.h"
#include "hbridge_motor.h"
#include "servo_gate_driver.h"
#include "button_driver.h"

class ActuationManager {
public:
    ActuationManager(ActuatorState& state,
                     HBridgeMotor& motor,
                     IServoGateDriver& servo,
                     DebouncedButton& button);

    // Initializes motor, servo, button, and sets normal operational states (motor 90%, servo closed)
    void begin();

    // Evaluates momentary push button with 50ms software debounce.
    // Toggles Machine Pause. On pause: cuts motor PWM to 0, locks servo.
    // On unpause: restores motor PWM to configured speed (90% for ON, 70% for MEDIUM), unlocks servo, preserves LED counts.
    // Returns true if a state transition occurred.
    bool handle_pause_button(uint32_t now_ms, bool raw_pressed);

    // Handles incoming servo command (SERVO_CLOSED=0, SERVO_OPEN=1).
    // If Machine Pause is active, command is suppressed and rejected (ADR-0004).
    // Returns true if a state transition occurred.
    bool handle_servo_command(uint8_t servo_state);

    // Handles incoming DC motor speed command (MOTOR_OFF=0, MOTOR_ON=1, MOTOR_MEDIUM=2).
    // If Machine Pause is active, command is suppressed and rejected (ADR-0004).
    // Returns true if a state transition occurred.
    bool handle_motor_command(uint8_t speed_state);

    // Direct accessors
    bool is_paused() const;
    bool is_motor_running() const;
    uint8_t get_motor_duty() const;
    uint8_t get_motor_state() const;
    uint8_t get_servo_state() const;
    int get_servo_angle() const;

private:
    ActuatorState& state_;
    HBridgeMotor& motor_;
    IServoGateDriver& servo_;
    DebouncedButton& button_;
};

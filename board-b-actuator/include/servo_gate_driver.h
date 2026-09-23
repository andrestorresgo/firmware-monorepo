#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

#define PIN_SERVO_GATE 13

// Abstract ServoGateDriver interface
class IServoGateDriver {
public:
    virtual ~IServoGateDriver() = default;
    virtual void begin() = 0;
    virtual void set_position(uint8_t position) = 0; // SERVO_CLOSED (0) -> 0 deg, SERVO_OPEN (1) -> 90 deg
    virtual uint8_t get_position() const = 0;
    virtual int get_angle() const = 0;
};

#ifndef UNIT_TEST

#include <Arduino.h>
#include <ESP32Servo.h>

// Concrete servo driver using ESP32Servo library on GPIO 13
class Esp32ServoGateDriver : public IServoGateDriver {
public:
    explicit Esp32ServoGateDriver(uint8_t pin = PIN_SERVO_GATE)
        : pin_(pin), position_(SERVO_CLOSED), angle_(0) {}

    void begin() override {
        // Allocate Timer 0 for ESP32PWM (LEDC Timer 0)
        ESP32PWM::allocateTimer(0);
        servo_.setPeriodHertz(50); // Standard 50Hz servo refresh rate
        servo_.attach(pin_, 500, 2400);
        set_position(SERVO_CLOSED);
    }

    void set_position(uint8_t position) override {
        position_ = (position == SERVO_OPEN) ? SERVO_OPEN : SERVO_CLOSED;
        angle_ = (position_ == SERVO_OPEN) ? 90 : 0;
        servo_.write(angle_);
    }

    uint8_t get_position() const override {
        return position_;
    }

    int get_angle() const override {
        return angle_;
    }

private:
    uint8_t pin_;
    uint8_t position_;
    int angle_;
    Servo servo_;
};

#else

// In-memory mock servo gate driver for native unit testing
class MockServoGateDriver : public IServoGateDriver {
public:
    MockServoGateDriver() : position_(SERVO_CLOSED), angle_(0), begin_called_(false) {}

    void begin() override {
        begin_called_ = true;
        set_position(SERVO_CLOSED);
    }

    void set_position(uint8_t position) override {
        position_ = (position == SERVO_OPEN) ? SERVO_OPEN : SERVO_CLOSED;
        angle_ = (position_ == SERVO_OPEN) ? 90 : 0;
    }

    uint8_t get_position() const override {
        return position_;
    }

    int get_angle() const override {
        return angle_;
    }

    bool was_begin_called() const {
        return begin_called_;
    }

private:
    uint8_t position_;
    int angle_;
    bool begin_called_;
};

#endif

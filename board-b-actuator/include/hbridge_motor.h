#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PIN_MOTOR_IN1 22
#define PIN_MOTOR_IN2 23

// Abstract HBridgeMotor class per Ticket 06
class HBridgeMotor {
public:
    virtual ~HBridgeMotor() = default;
    virtual void begin() = 0;
    virtual void drive_forward(uint8_t duty_percent = 80) = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
    virtual uint8_t get_duty_percent() const = 0;
};

#ifndef UNIT_TEST

#include <Arduino.h>

// Concrete hardware driver using ESP32 ledc PWM on GPIO 22 & GPIO 23
class Esp32HBridgeMotor : public HBridgeMotor {
public:
    Esp32HBridgeMotor(uint8_t pin_in1 = PIN_MOTOR_IN1,
                      uint8_t pin_in2 = PIN_MOTOR_IN2,
                      uint8_t ledc_channel = 4,
                      uint32_t freq_hz = 5000,
                      uint8_t resolution_bits = 8)
        : pin_in1_(pin_in1),
          pin_in2_(pin_in2),
          ledc_channel_(ledc_channel),
          freq_hz_(freq_hz),
          resolution_bits_(resolution_bits),
          running_(false),
          duty_percent_(0) {}

    void begin() override {
        pinMode(pin_in2_, OUTPUT);
        digitalWrite(pin_in2_, LOW);

        ledcSetup(ledc_channel_, freq_hz_, resolution_bits_);
        ledcAttachPin(pin_in1_, ledc_channel_);
        ledcWrite(ledc_channel_, 0);

        running_ = false;
        duty_percent_ = 0;
    }

    void drive_forward(uint8_t duty_percent = 80) override {
        if (duty_percent > 100) duty_percent = 100;
        duty_percent_ = duty_percent;
        running_ = (duty_percent > 0);

        digitalWrite(pin_in2_, LOW);
        uint32_t max_duty = (1 << resolution_bits_) - 1; // 255 for 8-bit resolution
        uint32_t duty_val = (max_duty * duty_percent) / 100;
        ledcWrite(ledc_channel_, duty_val);
    }

    void stop() override {
        duty_percent_ = 0;
        running_ = false;
        ledcWrite(ledc_channel_, 0);
        digitalWrite(pin_in2_, LOW);
    }

    bool is_running() const override {
        return running_;
    }

    uint8_t get_duty_percent() const override {
        return duty_percent_;
    }

private:
    uint8_t pin_in1_;
    uint8_t pin_in2_;
    uint8_t ledc_channel_;
    uint32_t freq_hz_;
    uint8_t resolution_bits_;
    bool running_;
    uint8_t duty_percent_;
};

#else

// In-memory mock driver for native unit testing
class MockHBridgeMotor : public HBridgeMotor {
public:
    MockHBridgeMotor() : running_(false), duty_percent_(0), begin_called_(false) {}

    void begin() override {
        begin_called_ = true;
        running_ = false;
        duty_percent_ = 0;
    }

    void drive_forward(uint8_t duty_percent = 80) override {
        if (duty_percent > 100) duty_percent = 100;
        duty_percent_ = duty_percent;
        running_ = (duty_percent > 0);
    }

    void stop() override {
        running_ = false;
        duty_percent_ = 0;
    }

    bool is_running() const override {
        return running_;
    }

    uint8_t get_duty_percent() const override {
        return duty_percent_;
    }

    bool was_begin_called() const {
        return begin_called_;
    }

private:
    bool running_;
    uint8_t duty_percent_;
    bool begin_called_;
};

#endif

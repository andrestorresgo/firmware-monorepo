#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "protocol.h"

#ifndef UNIT_TEST
#include <Arduino.h>
#endif

// Pin Definitions per spec.md:
// Red LEDs (Circle, Bits 0, 1, 2): GPIO 15, 2, 4
#define PIN_LED_RED_BIT0    15
#define PIN_LED_RED_BIT1    2
#define PIN_LED_RED_BIT2    4

// Green LEDs (Triangle, Bits 0, 1, 2): GPIO 16, 17, 5
#define PIN_LED_GREEN_BIT0  16
#define PIN_LED_GREEN_BIT1  17
#define PIN_LED_GREEN_BIT2  5

// Blue LEDs (Square, Bits 0, 1, 2): GPIO 18, 19, 21
#define PIN_LED_BLUE_BIT0   18
#define PIN_LED_BLUE_BIT1   19
#define PIN_LED_BLUE_BIT2   21

// Abstract interface for 3-bit binary LED banks
class ILedBankDriver {
public:
    virtual ~ILedBankDriver() = default;
    virtual void begin() = 0;
    virtual void write_bank(uint8_t shape_id, uint8_t pattern) = 0;
    virtual uint8_t get_bank_pattern(uint8_t shape_id) const = 0;
};

#ifndef UNIT_TEST

// Concrete hardware driver using ESP32 GPIOs in Active-HIGH configuration
class LedBankDriver : public ILedBankDriver {
public:
    LedBankDriver()
        : red_pattern_(0), green_pattern_(0), blue_pattern_(0) {}

    void begin() override {
        static const uint8_t pins[] = {
            PIN_LED_RED_BIT0, PIN_LED_RED_BIT1, PIN_LED_RED_BIT2,
            PIN_LED_GREEN_BIT0, PIN_LED_GREEN_BIT1, PIN_LED_GREEN_BIT2,
            PIN_LED_BLUE_BIT0, PIN_LED_BLUE_BIT1, PIN_LED_BLUE_BIT2
        };

        for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
            pinMode(pins[i], OUTPUT);
            digitalWrite(pins[i], LOW);
        }

        red_pattern_ = 0;
        green_pattern_ = 0;
        blue_pattern_ = 0;
    }

    void write_bank(uint8_t shape_id, uint8_t pattern) override {
        const uint8_t* pins = nullptr;
        uint8_t* pattern_ref = nullptr;

        static const uint8_t red_pins[] = {PIN_LED_RED_BIT0, PIN_LED_RED_BIT1, PIN_LED_RED_BIT2};
        static const uint8_t green_pins[] = {PIN_LED_GREEN_BIT0, PIN_LED_GREEN_BIT1, PIN_LED_GREEN_BIT2};
        static const uint8_t blue_pins[] = {PIN_LED_BLUE_BIT0, PIN_LED_BLUE_BIT1, PIN_LED_BLUE_BIT2};

        switch (shape_id) {
            case SHAPE_CIRCLE:
                pins = red_pins;
                pattern_ref = &red_pattern_;
                break;
            case SHAPE_TRIANGLE:
                pins = green_pins;
                pattern_ref = &green_pattern_;
                break;
            case SHAPE_SQUARE:
                pins = blue_pins;
                pattern_ref = &blue_pattern_;
                break;
            default:
                return;
        }

        *pattern_ref = pattern & 0x07;
        for (uint8_t bit = 0; bit < 3; ++bit) {
            bool state = (*pattern_ref & (1 << bit)) != 0;
            digitalWrite(pins[bit], state ? HIGH : LOW);
        }
    }

    uint8_t get_bank_pattern(uint8_t shape_id) const override {
        switch (shape_id) {
            case SHAPE_CIRCLE: return red_pattern_;
            case SHAPE_TRIANGLE: return green_pattern_;
            case SHAPE_SQUARE: return blue_pattern_;
            default: return 0;
        }
    }

private:
    uint8_t red_pattern_;
    uint8_t green_pattern_;
    uint8_t blue_pattern_;
};

#else

// In-memory mock driver for native unit tests
class MockLedBankDriver : public ILedBankDriver {
public:
    MockLedBankDriver()
        : red_pattern_(0), green_pattern_(0), blue_pattern_(0), begin_called_(false) {}

    void begin() override {
        begin_called_ = true;
        red_pattern_ = 0;
        green_pattern_ = 0;
        blue_pattern_ = 0;
    }

    void write_bank(uint8_t shape_id, uint8_t pattern) override {
        uint8_t masked = pattern & 0x07;
        switch (shape_id) {
            case SHAPE_CIRCLE: red_pattern_ = masked; break;
            case SHAPE_TRIANGLE: green_pattern_ = masked; break;
            case SHAPE_SQUARE: blue_pattern_ = masked; break;
            default: break;
        }
    }

    uint8_t get_bank_pattern(uint8_t shape_id) const override {
        switch (shape_id) {
            case SHAPE_CIRCLE: return red_pattern_;
            case SHAPE_TRIANGLE: return green_pattern_;
            case SHAPE_SQUARE: return blue_pattern_;
            default: return 0;
        }
    }

    bool was_begin_called() const { return begin_called_; }

private:
    uint8_t red_pattern_;
    uint8_t green_pattern_;
    uint8_t blue_pattern_;
    bool begin_called_;
};

#endif

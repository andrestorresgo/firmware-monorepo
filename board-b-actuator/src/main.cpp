#include <Arduino.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include "protocol.h"

void setup() {
    Serial.begin(115200);
    Serial.println(F("[Actuator] Firmware initialized"));
}

void loop() {
    delay(1000);
}
#include <Arduino.h>
#include <U8g2lib.h>
#include <Keypad.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "protocol.h"

void setup() {
    Serial.begin(115200);
    Serial.println(F("[Gateway] Firmware initialized"));
}

void loop() {
    delay(1000);
}
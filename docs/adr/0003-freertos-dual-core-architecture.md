# FreeRTOS Dual-Core Concurrency Architecture

We decided to divide firmware responsibilities across the ESP32's dual cores using FreeRTOS tasks and queues on both microcontrollers. Core 0 handles network I/O (TLS MQTT and ESP-NOW transport), while Core 1 handles real-time peripheral operations (keypad scanning, OLED refresh, DC motor PWM, and safety button debouncing), preventing network handshake latency from causing user interface stutter or delayed safety halts.

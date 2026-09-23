#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Framing Constants
#define ESPNOW_MAGIC_BYTE 0xA5
#define MAX_PAYLOAD_SIZE  32

// Message Opcodes
enum MessageOpcode {
    OPCODE_BEACON          = 0x01, // Gateway broadcasts Wi-Fi RF channel & MAC
    OPCODE_BEACON_ACK      = 0x02, // Actuator replies acknowledging discovery
    OPCODE_SHAPE_DETECTION = 0x03, // Gateway commands shape counter increment
    OPCODE_SERVO_COMMAND   = 0x04, // Gateway commands sorting gate position
    OPCODE_TELEMETRY       = 0x05, // Actuator transmits status heartbeat
    OPCODE_BATCH_ROLLOVER  = 0x06  // Actuator notifies batch rollover completion
};

// Shape Identifiers (Circle=Red, Triangle=Green, Square=Blue)
enum ShapeId {
    SHAPE_UNKNOWN  = 0,
    SHAPE_CIRCLE   = 1, // Red LED bank (bits 0, 1, 2)
    SHAPE_TRIANGLE = 2, // Green LED bank (bits 0, 1, 2)
    SHAPE_SQUARE   = 3  // Blue LED bank (bits 0, 1, 2)
};

// Servo Positions
enum ServoPosition {
    SERVO_CLOSED = 0, // Gate closed (0 degrees)
    SERVO_OPEN   = 1  // Gate open (90 degrees)
};

#pragma pack(push, 1)

// Common Frame Header (4 bytes)
struct FrameHeader {
    uint8_t magic;       // Must equal ESPNOW_MAGIC_BYTE (0xA5)
    uint8_t opcode;      // One of MessageOpcode
    uint8_t payload_len; // Length of following payload in bytes
    uint8_t checksum;    // Checksum over payload bytes
};

// Payload for OPCODE_BEACON (11 bytes)
struct BeaconPayload {
    uint8_t wifi_channel;    // Operating RF channel (1-13)
    uint8_t gateway_mac[6];  // Gateway MAC address
    uint32_t uptime_ms;      // Gateway uptime in milliseconds
};

// Payload for OPCODE_BEACON_ACK (7 bytes)
struct BeaconAckPayload {
    uint8_t actuator_mac[6]; // Actuator MAC address
    uint8_t status;          // 0 = Paired OK, 1 = Error
};

// Payload for OPCODE_SHAPE_DETECTION (5 bytes)
struct ShapeDetectionPayload {
    uint8_t shape_id;        // 1=Circle, 2=Triangle, 3=Square
    uint32_t detection_id;   // Sequence or unique detection id
};

// Payload for OPCODE_SERVO_COMMAND (1 byte)
struct ServoCommandPayload {
    uint8_t servo_state;     // 0 = SERVO_CLOSED, 1 = SERVO_OPEN
};

// Payload for OPCODE_TELEMETRY (10 bytes)
struct TelemetryPayload {
    uint8_t is_paused;       // 1 = Machine paused, 0 = Active
    uint8_t motor_state;     // 1 = Motor running, 0 = Motor off
    uint8_t servo_state;     // 0 = SERVO_CLOSED, 1 = SERVO_OPEN
    uint8_t red_count;       // Current count for Red/Circle (0-5)
    uint8_t green_count;     // Current count for Green/Triangle (0-5)
    uint8_t blue_count;      // Current count for Blue/Square (0-5)
    uint32_t uptime_ms;      // Actuator uptime in milliseconds
};

// Payload for OPCODE_BATCH_ROLLOVER (6 bytes)
struct BatchRolloverPayload {
    uint8_t shape_id;        // Shape that completed cycle (1, 2, or 3)
    uint8_t batch_size;      // Batch quantity (default 5)
    uint32_t timestamp_ms;   // Actuator timestamp of rollover
};

// Unified ESP-NOW Packet Container
struct EspNowPacket {
    FrameHeader header;
    union {
        BeaconPayload beacon;
        BeaconAckPayload beacon_ack;
        ShapeDetectionPayload shape_detection;
        ServoCommandPayload servo_command;
        TelemetryPayload telemetry;
        BatchRolloverPayload batch_rollover;
        uint8_t raw[MAX_PAYLOAD_SIZE];
    } payload;
};

#pragma pack(pop)

// Protocol Helper Functions

// Calculate checksum with 8-bit rotating XOR
static inline uint8_t calculate_checksum(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;
    uint8_t sum = 0x5A; // non-zero seed
    for (size_t i = 0; i < len; ++i) {
        sum ^= data[i];
        sum = (uint8_t)((sum << 1) | (sum >> 7));
    }
    return sum;
}

// Validate frame boundaries, magic byte, opcode, payload length, and checksum
static inline bool validate_frame(const uint8_t* buffer, size_t len) {
    if (!buffer || len < sizeof(FrameHeader)) {
        return false;
    }
    const struct FrameHeader* hdr = (const struct FrameHeader*)buffer;
    if (hdr->magic != ESPNOW_MAGIC_BYTE) {
        return false;
    }
    if (hdr->payload_len > MAX_PAYLOAD_SIZE) {
        return false;
    }
    if (len < sizeof(struct FrameHeader) + hdr->payload_len) {
        return false;
    }
    
    // Check expected length per opcode
    switch (hdr->opcode) {
        case OPCODE_BEACON:
            if (hdr->payload_len != sizeof(struct BeaconPayload)) return false;
            break;
        case OPCODE_BEACON_ACK:
            if (hdr->payload_len != sizeof(struct BeaconAckPayload)) return false;
            break;
        case OPCODE_SHAPE_DETECTION:
            if (hdr->payload_len != sizeof(struct ShapeDetectionPayload)) return false;
            break;
        case OPCODE_SERVO_COMMAND:
            if (hdr->payload_len != sizeof(struct ServoCommandPayload)) return false;
            break;
        case OPCODE_TELEMETRY:
            if (hdr->payload_len != sizeof(struct TelemetryPayload)) return false;
            break;
        case OPCODE_BATCH_ROLLOVER:
            if (hdr->payload_len != sizeof(struct BatchRolloverPayload)) return false;
            break;
        default:
            return false;
    }

    // Check payload checksum
    const uint8_t* payload_ptr = buffer + sizeof(struct FrameHeader);
    uint8_t expected_checksum = calculate_checksum(payload_ptr, hdr->payload_len);
    if (hdr->checksum != expected_checksum) {
        return false;
    }

    return true;
}

// Pack an EspNowPacket into a raw byte buffer ready for transmission
static inline int pack_packet(const struct EspNowPacket* packet, uint8_t* out_buf, size_t max_len) {
    if (!packet || !out_buf) return -1;
    size_t total_len = sizeof(struct FrameHeader) + packet->header.payload_len;
    if (max_len < total_len || packet->header.payload_len > MAX_PAYLOAD_SIZE) {
        return -1;
    }
    
    struct FrameHeader* out_hdr = (struct FrameHeader*)out_buf;
    out_hdr->magic = ESPNOW_MAGIC_BYTE;
    out_hdr->opcode = packet->header.opcode;
    out_hdr->payload_len = packet->header.payload_len;
    
    if (packet->header.payload_len > 0) {
        memcpy(out_buf + sizeof(struct FrameHeader), packet->payload.raw, packet->header.payload_len);
    }
    out_hdr->checksum = calculate_checksum(out_buf + sizeof(struct FrameHeader), out_hdr->payload_len);
    
    return (int)total_len;
}

// Unpack and validate a raw byte buffer into an EspNowPacket
static inline bool unpack_packet(const uint8_t* in_buf, size_t in_len, struct EspNowPacket* out_packet) {
    if (!in_buf || !out_packet) return false;
    if (!validate_frame(in_buf, in_len)) return false;
    
    memset(out_packet, 0, sizeof(struct EspNowPacket));
    const struct FrameHeader* in_hdr = (const struct FrameHeader*)in_buf;
    out_packet->header = *in_hdr;
    
    if (in_hdr->payload_len > 0) {
        memcpy(out_packet->payload.raw, in_buf + sizeof(struct FrameHeader), in_hdr->payload_len);
    }
    return true;
}

#ifdef __cplusplus
}
#endif

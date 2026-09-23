#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "protocol.h"

enum LinkState {
    LINK_STATE_SCANNING = 0,
    LINK_STATE_PAIRED   = 1,
    LINK_STATE_LOST     = 2
};

class LinkManager {
public:
    explicit LinkManager(uint32_t dwell_time_ms = 120,
                         uint32_t heartbeat_interval_ms = 3000,
                         uint32_t link_timeout_ms = 7000,
                         uint8_t max_consecutive_failures = 3);

    void begin(uint32_t now_ms = 0);

    // Drives channel scanning dwell timer.
    // If scanning and dwell expired, advances channel and returns true with out_new_channel.
    bool tick(uint32_t now_ms, uint8_t* out_new_channel);

    // Processes an incoming discovery beacon from Gateway.
    // Validates packet, locks channel, copies Gateway MAC, and transitions state to PAIRED.
    bool handle_beacon(const struct BeaconPayload* beacon, const uint8_t* sender_mac, uint32_t now_ms);

    // Builds a validated OPCODE_BEACON_ACK packet
    int build_beacon_ack(const uint8_t* my_mac, struct EspNowPacket* out_packet) const;

    // Checks if periodic 3-second heartbeat is due
    bool should_send_heartbeat(uint32_t now_ms) const;
    void record_heartbeat_sent(uint32_t now_ms);

    // Link health monitoring
    void record_activity(uint32_t now_ms);
    void record_send_failure();
    void record_send_success();
    bool check_link_timeout(uint32_t now_ms);

    // State queries
    LinkState get_state() const;
    bool is_paired() const;
    uint8_t get_current_channel() const;
    const uint8_t* get_gateway_mac() const;

private:
    uint32_t dwell_time_ms_;
    uint32_t heartbeat_interval_ms_;
    uint32_t link_timeout_ms_;
    uint8_t max_consecutive_failures_;

    LinkState state_;
    uint8_t current_channel_;
    uint8_t gateway_mac_[6];
    uint32_t last_hop_ms_;
    uint32_t last_heartbeat_ms_;
    uint32_t last_activity_ms_;
    uint8_t consecutive_failures_;

    void transition_to_scanning(uint32_t now_ms);
};

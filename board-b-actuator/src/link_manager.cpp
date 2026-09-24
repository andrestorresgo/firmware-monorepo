#include "link_manager.h"
#include <string.h>

LinkManager::LinkManager(uint32_t dwell_time_ms,
                         uint32_t heartbeat_interval_ms,
                         uint32_t link_timeout_ms,
                         uint8_t max_consecutive_failures)
    : dwell_time_ms_(dwell_time_ms),
      heartbeat_interval_ms_(heartbeat_interval_ms),
      link_timeout_ms_(link_timeout_ms),
      max_consecutive_failures_(max_consecutive_failures),
      state_(LINK_STATE_SCANNING),
      current_channel_(1),
      last_hop_ms_(0),
      last_heartbeat_ms_(0),
      last_activity_ms_(0),
      consecutive_failures_(0) {
    memset(gateway_mac_, 0, sizeof(gateway_mac_));
}

void LinkManager::begin(uint32_t now_ms) {
    state_ = LINK_STATE_SCANNING;
    current_channel_ = 1;
    last_hop_ms_ = now_ms;
    last_heartbeat_ms_ = 0;
    last_activity_ms_ = now_ms;
    consecutive_failures_ = 0;
    memset(gateway_mac_, 0, sizeof(gateway_mac_));
}

bool LinkManager::tick(uint32_t now_ms, uint8_t* out_new_channel) {
    if (state_ != LINK_STATE_SCANNING) {
        return false;
    }

    if (now_ms - last_hop_ms_ >= dwell_time_ms_) {
        last_hop_ms_ = now_ms;
        // Advance channel 1..13 with wrap-around
        current_channel_ = (current_channel_ % 13) + 1;
        if (out_new_channel) {
            *out_new_channel = current_channel_;
        }
        return true;
    }

    return false;
}

bool LinkManager::handle_beacon(const struct BeaconPayload* beacon, const uint8_t* sender_mac, uint32_t now_ms) {
    if (!beacon || !sender_mac) {
        return false;
    }

    // Validate 2.4 GHz Wi-Fi channel
    if (beacon->wifi_channel < 1 || beacon->wifi_channel > 13) {
        return false;
    }

    // Lock to Gateway's operating channel and MAC
    current_channel_ = beacon->wifi_channel;
    memcpy(gateway_mac_, beacon->gateway_mac, 6);

    if (state_ != LINK_STATE_PAIRED) {
        state_ = LINK_STATE_PAIRED;
        last_heartbeat_ms_ = now_ms;
    }
    last_activity_ms_ = now_ms;
    consecutive_failures_ = 0;

    return true;
}

int LinkManager::build_beacon_ack(const uint8_t* my_mac, struct EspNowPacket* out_packet) const {
    if (!my_mac || !out_packet) {
        return -1;
    }

    memset(out_packet, 0, sizeof(struct EspNowPacket));
    out_packet->header.magic = ESPNOW_MAGIC_BYTE;
    out_packet->header.opcode = OPCODE_BEACON_ACK;
    out_packet->header.payload_len = sizeof(struct BeaconAckPayload);

    memcpy(out_packet->payload.beacon_ack.actuator_mac, my_mac, 6);
    out_packet->payload.beacon_ack.status = 0; // 0 = Paired OK

    return sizeof(struct FrameHeader) + sizeof(struct BeaconAckPayload);
}

bool LinkManager::should_send_heartbeat(uint32_t now_ms) const {
    if (state_ != LINK_STATE_PAIRED) {
        return false;
    }

    return (now_ms - last_heartbeat_ms_ >= heartbeat_interval_ms_);
}

void LinkManager::record_heartbeat_sent(uint32_t now_ms) {
    last_heartbeat_ms_ = now_ms;
}

void LinkManager::record_activity(uint32_t now_ms) {
    last_activity_ms_ = now_ms;
    consecutive_failures_ = 0;
}

void LinkManager::record_send_failure() {
    consecutive_failures_++;
    if (consecutive_failures_ >= max_consecutive_failures_) {
        transition_to_scanning(last_activity_ms_);
    }
}

void LinkManager::record_send_success() {
    consecutive_failures_ = 0;
}

bool LinkManager::check_link_timeout(uint32_t now_ms) {
    if (state_ == LINK_STATE_PAIRED && (now_ms - last_activity_ms_ >= link_timeout_ms_)) {
        transition_to_scanning(now_ms);
        return true;
    }
    return false;
}

void LinkManager::transition_to_scanning(uint32_t now_ms) {
    state_ = LINK_STATE_SCANNING;
    consecutive_failures_ = 0;
    last_hop_ms_ = now_ms;
    memset(gateway_mac_, 0, sizeof(gateway_mac_));
}

LinkState LinkManager::get_state() const {
    return state_;
}

bool LinkManager::is_paired() const {
    return (state_ == LINK_STATE_PAIRED);
}

uint8_t LinkManager::get_current_channel() const {
    return current_channel_;
}

const uint8_t* LinkManager::get_gateway_mac() const {
    return gateway_mac_;
}

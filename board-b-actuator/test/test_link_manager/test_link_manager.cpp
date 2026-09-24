#include <unity.h>
#include <string.h>
#include "link_manager.h"
#include "protocol.h"

void setUp(void) {}
void tearDown(void) {}

void test_link_manager_initial_state(void) {
    LinkManager lm;
    TEST_ASSERT_EQUAL(LINK_STATE_SCANNING, lm.get_state());
    TEST_ASSERT_FALSE(lm.is_paired());
    TEST_ASSERT_EQUAL_UINT8(1, lm.get_current_channel());
}

void test_channel_sweep_progression(void) {
    LinkManager lm(100); // 100ms dwell time
    lm.begin(0);

    // At 50ms, should not hop
    uint8_t new_ch = 0;
    TEST_ASSERT_FALSE(lm.tick(50, &new_ch));
    TEST_ASSERT_EQUAL_UINT8(1, lm.get_current_channel());

    // At 100ms, should hop to channel 2
    TEST_ASSERT_TRUE(lm.tick(100, &new_ch));
    TEST_ASSERT_EQUAL_UINT8(2, new_ch);
    TEST_ASSERT_EQUAL_UINT8(2, lm.get_current_channel());

    // Advance through all 13 channels
    for (uint8_t expected_ch = 3; expected_ch <= 13; ++expected_ch) {
        uint32_t t = 100 * (expected_ch - 1);
        TEST_ASSERT_TRUE(lm.tick(t, &new_ch));
        TEST_ASSERT_EQUAL_UINT8(expected_ch, new_ch);
    }

    // After channel 13, wrap around to channel 1
    TEST_ASSERT_TRUE(lm.tick(1300, &new_ch));
    TEST_ASSERT_EQUAL_UINT8(1, new_ch);
    TEST_ASSERT_EQUAL_UINT8(1, lm.get_current_channel());
}

void test_beacon_discovery_and_channel_locking(void) {
    LinkManager lm(120);
    lm.begin(0);

    // Hop a couple channels
    uint8_t ch = 0;
    lm.tick(120, &ch); // ch 2
    lm.tick(240, &ch); // ch 3
    TEST_ASSERT_EQUAL_UINT8(3, lm.get_current_channel());

    // Receive beacon from Gateway operating on channel 6
    uint8_t gw_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    BeaconPayload beacon = {};
    beacon.wifi_channel = 6;
    memcpy(beacon.gateway_mac, gw_mac, 6);
    beacon.uptime_ms = 45000;

    bool accepted = lm.handle_beacon(&beacon, gw_mac, 260);
    TEST_ASSERT_TRUE(accepted);
    TEST_ASSERT_TRUE(lm.is_paired());
    TEST_ASSERT_EQUAL(LINK_STATE_PAIRED, lm.get_state());
    TEST_ASSERT_EQUAL_UINT8(6, lm.get_current_channel());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(gw_mac, lm.get_gateway_mac(), 6);

    // While paired, tick() should NOT advance channel
    TEST_ASSERT_FALSE(lm.tick(1000, &ch));
    TEST_ASSERT_EQUAL_UINT8(6, lm.get_current_channel());
}

void test_beacon_validation_errors(void) {
    LinkManager lm;
    lm.begin(0);

    uint8_t gw_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    BeaconPayload beacon = {};
    beacon.wifi_channel = 6;
    memcpy(beacon.gateway_mac, gw_mac, 6);

    // NULL beacon
    TEST_ASSERT_FALSE(lm.handle_beacon(NULL, gw_mac, 100));

    // Invalid channels (0 or >13)
    beacon.wifi_channel = 0;
    TEST_ASSERT_FALSE(lm.handle_beacon(&beacon, gw_mac, 100));
    beacon.wifi_channel = 14;
    TEST_ASSERT_FALSE(lm.handle_beacon(&beacon, gw_mac, 100));

    TEST_ASSERT_FALSE(lm.is_paired());
}

void test_build_beacon_ack_packet(void) {
    LinkManager lm;
    uint8_t my_mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    EspNowPacket pkt = {};
    int len = lm.build_beacon_ack(my_mac, &pkt);

    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL(sizeof(FrameHeader) + sizeof(BeaconAckPayload), (size_t)len);
    TEST_ASSERT_EQUAL_HEX8(ESPNOW_MAGIC_BYTE, pkt.header.magic);
    TEST_ASSERT_EQUAL_HEX8(OPCODE_BEACON_ACK, pkt.header.opcode);
    TEST_ASSERT_EQUAL_UINT8(sizeof(BeaconAckPayload), pkt.header.payload_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(my_mac, pkt.payload.beacon_ack.actuator_mac, 6);
    TEST_ASSERT_EQUAL_UINT8(0, pkt.payload.beacon_ack.status);

    // Validate packing
    uint8_t raw[64];
    int packed_len = pack_packet(&pkt, raw, sizeof(raw));
    TEST_ASSERT_GREATER_THAN(0, packed_len);
    TEST_ASSERT_TRUE(validate_frame(raw, (size_t)packed_len));
}

void test_heartbeat_cadence_3_seconds(void) {
    LinkManager lm;
    lm.begin(0);

    uint8_t gw_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    BeaconPayload beacon = {};
    beacon.wifi_channel = 1;
    memcpy(beacon.gateway_mac, gw_mac, 6);

    lm.handle_beacon(&beacon, gw_mac, 1000);

    // Initial heartbeat shouldn't fire immediately at 1000ms
    TEST_ASSERT_FALSE(lm.should_send_heartbeat(2000));
    TEST_ASSERT_FALSE(lm.should_send_heartbeat(3999));

    // At 4000ms (1000 + 3000ms), heartbeat is due
    TEST_ASSERT_TRUE(lm.should_send_heartbeat(4000));

    // Record heartbeat sent
    lm.record_heartbeat_sent(4000);
    TEST_ASSERT_FALSE(lm.should_send_heartbeat(4000));
    TEST_ASSERT_FALSE(lm.should_send_heartbeat(6999));
    TEST_ASSERT_TRUE(lm.should_send_heartbeat(7000));
}

void test_heartbeat_not_starved_by_intermediate_beacons(void) {
    LinkManager lm;
    lm.begin(0);

    uint8_t gw_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    BeaconPayload beacon = {};
    beacon.wifi_channel = 1;
    memcpy(beacon.gateway_mac, gw_mac, 6);

    // Initial pairing beacon at 1000ms
    lm.handle_beacon(&beacon, gw_mac, 1000);

    // Intermediate beacons from Gateway arriving at 2000ms and 3000ms
    lm.handle_beacon(&beacon, gw_mac, 2000);
    lm.handle_beacon(&beacon, gw_mac, 3000);

    // Heartbeat MUST still be due at 4000ms (1000ms initial + 3000ms interval)
    // Intermediate beacons must not starve the Actuator's periodic telemetry heartbeat!
    TEST_ASSERT_TRUE(lm.should_send_heartbeat(4000));
}

void test_link_loss_timeout_and_rescan(void) {
    LinkManager lm;
    lm.begin(0);

    uint8_t gw_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    BeaconPayload beacon = {};
    beacon.wifi_channel = 5;
    memcpy(beacon.gateway_mac, gw_mac, 6);

    lm.handle_beacon(&beacon, gw_mac, 1000);
    TEST_ASSERT_TRUE(lm.is_paired());

    // Inactivity under timeout (e.g. at 5000ms, diff is 4000ms < 7000ms)
    TEST_ASSERT_FALSE(lm.check_link_timeout(5000));
    TEST_ASSERT_TRUE(lm.is_paired());

    // Receiving activity refreshes timeout
    lm.record_activity(6000);

    // Timeout exceeded (6000 + 7001ms = 13001ms)
    TEST_ASSERT_TRUE(lm.check_link_timeout(13001));
    TEST_ASSERT_FALSE(lm.is_paired());
    TEST_ASSERT_EQUAL(LINK_STATE_SCANNING, lm.get_state());

    // Resuming scanning continues hopping without crashing
    uint8_t new_ch = 0;
    lm.tick(13150, &new_ch);
}

void test_consecutive_send_failures_trigger_rescan(void) {
    LinkManager lm;
    lm.begin(0);

    uint8_t gw_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    BeaconPayload beacon = {};
    beacon.wifi_channel = 11;
    memcpy(beacon.gateway_mac, gw_mac, 6);

    lm.handle_beacon(&beacon, gw_mac, 1000);
    TEST_ASSERT_TRUE(lm.is_paired());

    lm.record_send_failure();
    TEST_ASSERT_TRUE(lm.is_paired());

    lm.record_send_failure();
    TEST_ASSERT_TRUE(lm.is_paired());

    // 3rd consecutive failure triggers link loss
    lm.record_send_failure();
    TEST_ASSERT_FALSE(lm.is_paired());
    TEST_ASSERT_EQUAL(LINK_STATE_SCANNING, lm.get_state());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_link_manager_initial_state);
    RUN_TEST(test_channel_sweep_progression);
    RUN_TEST(test_beacon_discovery_and_channel_locking);
    RUN_TEST(test_beacon_validation_errors);
    RUN_TEST(test_build_beacon_ack_packet);
    RUN_TEST(test_heartbeat_cadence_3_seconds);
    RUN_TEST(test_heartbeat_not_starved_by_intermediate_beacons);
    RUN_TEST(test_link_loss_timeout_and_rescan);
    RUN_TEST(test_consecutive_send_failures_trigger_rescan);
    return UNITY_END();
}

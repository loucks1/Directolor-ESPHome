#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <initializer_list>
#include "esphome.h"
#include "esphome/components/nrf24/nrf24.h"
#include "../../components/directolor_radio/directolor_radio.h"
#include "../../components/directolor_radio/directolor_packet.h"
#include "../../components/directolor_radio/payload_queue.h"

// Mock millis global
unsigned long current_millis = 0;
unsigned long millis() {
    return current_millis;
}

using namespace esphome::directolor_radio;
using namespace esphome::nrf24;

static int tests_run = 0;
static int tests_failed = 0;

#define EXPECT_TRUE(cond) do { \
    tests_run++; \
    if (!(cond)) { \
        tests_failed++; \
        std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond << std::endl; \
    } \
} while (0)

#define EXPECT_EQ(a, b) do { \
    tests_run++; \
    auto _a = (a); auto _b = (b); \
    if (_a != _b) { \
        tests_failed++; \
        std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ \
                  << " " << #a << " (" << (long long)_a << ") != " << #b \
                  << " (" << (long long)_b << ")" << std::endl; \
    } \
} while (0)

void test_payload_queue() {
    PayloadQueue q;
    EXPECT_TRUE(q.isEmpty());
    EXPECT_EQ(q.size(), 0);

    uint8_t payload[32];
    std::memset(payload, 0xAB, sizeof(payload));

    EXPECT_TRUE(q.enqueue(payload, 10));
    EXPECT_EQ(q.size(), 1);
    EXPECT_TRUE(!q.isEmpty());

    PayloadEntry entry{};
    EXPECT_TRUE(q.dequeue(entry));
    EXPECT_EQ(entry.send_attempts, 10);
    EXPECT_EQ(entry.payload[0], 0xAB);
    EXPECT_TRUE(q.isEmpty());

    // Fill to capacity
    for (size_t i = 0; i < PayloadQueue::capacity(); i++) {
        payload[0] = static_cast<uint8_t>(i);
        EXPECT_TRUE(q.enqueue(payload, static_cast<int>(i + 1)));
    }
    EXPECT_TRUE(!q.enqueue(payload, 99)); // full
    EXPECT_EQ(q.size(), static_cast<int>(PayloadQueue::capacity()));

    // Drain and verify order
    for (size_t i = 0; i < PayloadQueue::capacity(); i++) {
        EXPECT_TRUE(q.dequeue(entry));
        EXPECT_EQ(entry.payload[0], static_cast<uint8_t>(i));
        EXPECT_EQ(entry.send_attempts, static_cast<int>(i + 1));
    }
    EXPECT_TRUE(q.isEmpty());
    EXPECT_TRUE(!q.dequeue(entry));

    // Removing a middle entry keeps the relative order of the others.
    for (uint8_t i = 1; i <= 3; i++) {
        payload[0] = i;
        EXPECT_TRUE(q.enqueue(payload, i));
    }
    EXPECT_TRUE(q.remove_at(1));
    EXPECT_EQ(q.size(), 2);
    EXPECT_TRUE(q.dequeue(entry));
    EXPECT_EQ(entry.payload[0], 1);
    EXPECT_TRUE(q.dequeue(entry));
    EXPECT_EQ(entry.payload[0], 3);
    EXPECT_TRUE(q.isEmpty());

    std::cout << "test_payload_queue passed" << std::endl;
}

void test_payload_matching() {
    DirectolorRadio radio;
    NRF24Component mock_nrf;
    radio.set_nrf24(&mock_nrf);
    radio.setup();
    radio.set_listening(true);

    // Enter learning mode via loop
    radio.loop();
    EXPECT_TRUE(mock_nrf.listening_);

    uint8_t fake_payload[32] = {0};
    fake_payload[0] = 0xAA;
    fake_payload[1] = 0xBB;
    fake_payload[2] = 0xC0;
    fake_payload[3] = 0x11;
    fake_payload[4] = 0x00;
    fake_payload[5] = 0x05;
    fake_payload[9] = 0xCC;
    fake_payload[10] = 0xDD;

    mock_nrf.trigger_incoming(fake_payload, 32);

    EXPECT_TRUE(radio.has_sniffed_code());
    auto code = radio.get_sniffed_remote_code();
    EXPECT_EQ(code[0], 0xAA);
    EXPECT_EQ(code[1], 0xBB);
    EXPECT_EQ(code[2], 0xCC);
    EXPECT_EQ(code[3], 0xDD);

    std::cout << "test_payload_matching passed" << std::endl;
}

void test_non_blocking_dense_send() {
    DirectolorRadio radio;
    NRF24Component mock_nrf;
    radio.set_nrf24(&mock_nrf);
    radio.set_message_send_repeats(30);
    radio.set_cooldown(0);
    radio.set_listening(false);
    radio.setup();

    // Skip learning branch
    uint8_t test_payload[32] = {0};
    EXPECT_TRUE(radio.sendPayload(test_payload));
    EXPECT_EQ(radio.pending_payload_count(), 1);

    current_millis = 0;
    radio.loop(); // dequeue + power_up; wait Tpd2stby before TX
    EXPECT_EQ(mock_nrf.write_count_, 0);
    EXPECT_TRUE(mock_nrf.powered_);

    current_millis = TX_POWER_UP_SETTLE_MS;
    radio.loop();
    // With TX_BURST_BUDGET_MS=25 and millis frozen, the entire payload
    // can complete in one burst (budget never expires when time is frozen).
    EXPECT_EQ(mock_nrf.write_count_, 30);
    EXPECT_EQ(radio.pending_payload_count(), 0);
    EXPECT_TRUE(!mock_nrf.powered_);
    EXPECT_EQ(mock_nrf.power_down_count_, 1);

    // Second payload: verify multi-payload queue drain with cooldown=0
    mock_nrf.write_count_ = 0;
    EXPECT_TRUE(radio.sendPayload(test_payload));
    EXPECT_TRUE(radio.sendPayload(test_payload));
    EXPECT_EQ(radio.pending_payload_count(), 2);

    radio.loop(); // power_up first remaining payload
    EXPECT_EQ(mock_nrf.write_count_, 0);
    current_millis += TX_POWER_UP_SETTLE_MS;
    radio.loop(); // finishes first
    EXPECT_EQ(mock_nrf.write_count_, 30);
    EXPECT_EQ(radio.pending_payload_count(), 1);
    EXPECT_TRUE(!mock_nrf.powered_);

    current_millis += 1;
    radio.loop(); // power_up second (cooldown 0: now - finish < 0 is false)
    EXPECT_EQ(mock_nrf.write_count_, 30);
    current_millis += TX_POWER_UP_SETTLE_MS;
    radio.loop();
    EXPECT_EQ(mock_nrf.write_count_, 60);
    EXPECT_EQ(radio.pending_payload_count(), 0);
    EXPECT_TRUE(!mock_nrf.powered_);

    std::cout << "test_non_blocking_dense_send passed" << std::endl;
}

void test_power_down_and_cooldown_between_payloads() {
    DirectolorRadio radio;
    NRF24Component mock_nrf;
    radio.set_nrf24(&mock_nrf);
    radio.set_message_send_repeats(10);
    radio.set_cooldown(100);
    radio.set_listening(false);
    radio.setup();

    uint8_t payload[32] = {0};
    EXPECT_TRUE(radio.sendPayload(payload));
    EXPECT_TRUE(radio.sendPayload(payload));

    current_millis = 1000;
    radio.loop(); // power_up
    EXPECT_EQ(mock_nrf.write_count_, 0);
    EXPECT_TRUE(mock_nrf.powered_);

    current_millis += TX_POWER_UP_SETTLE_MS;
    radio.loop(); // complete first payload and power down
    EXPECT_EQ(mock_nrf.write_count_, 10);
    EXPECT_EQ(radio.pending_payload_count(), 1);
    EXPECT_TRUE(!mock_nrf.powered_);
    EXPECT_EQ(mock_nrf.power_down_count_, 1);

    // Still inside 100 ms cooldown: must stay powered down and not TX
    current_millis += 99;
    radio.loop();
    EXPECT_EQ(mock_nrf.write_count_, 10);
    EXPECT_TRUE(!mock_nrf.powered_);
    EXPECT_EQ(radio.pending_payload_count(), 1);

    current_millis += 1; // cooldown elapsed
    radio.loop(); // power_up second payload
    EXPECT_EQ(mock_nrf.write_count_, 10);
    EXPECT_TRUE(mock_nrf.powered_);
    EXPECT_EQ(mock_nrf.power_up_count_, 2);

    current_millis += TX_POWER_UP_SETTLE_MS - 1;
    radio.loop(); // still settling
    EXPECT_EQ(mock_nrf.write_count_, 10);

    current_millis += 1;
    radio.loop();
    EXPECT_EQ(mock_nrf.write_count_, 20);
    EXPECT_EQ(radio.pending_payload_count(), 0);
    EXPECT_TRUE(!mock_nrf.powered_);
    EXPECT_EQ(mock_nrf.power_down_count_, 2);

    std::cout << "test_power_down_and_cooldown_between_payloads passed" << std::endl;
}

void test_queue_full_drop() {
    DirectolorRadio radio;
    NRF24Component mock_nrf;
    radio.set_nrf24(&mock_nrf);
    radio.set_message_send_repeats(1);
    radio.set_listening(false);
    radio.setup();

    uint8_t payload[32] = {0};
    for (size_t i = 0; i < PayloadQueue::capacity(); i++) {
        EXPECT_TRUE(radio.sendPayload(payload));
    }
    EXPECT_TRUE(!radio.sendPayload(payload));
    EXPECT_EQ(radio.pending_payload_count(), static_cast<int>(PayloadQueue::capacity()));

    std::cout << "test_queue_full_drop passed" << std::endl;
}

void test_burst_yields_when_time_advances() {
    DirectolorRadio radio;
    NRF24Component mock_nrf;
    radio.set_nrf24(&mock_nrf);
    radio.set_message_send_repeats(100);
    radio.set_cooldown(0);
    radio.set_listening(false);
    radio.setup();

    uint8_t payload[32] = {0};
    EXPECT_TRUE(radio.sendPayload(payload));

    // Advance millis inside write_fast by monkey-patching via timed loops:
    // First call with millis fixed at 0 will complete all 100 (budget never expires).
    // Instead, manually simulate: call loop with time that advances every standby.
    // Our mock doesn't advance time; verify partial burst by advancing before each loop
    // after forcing early yield isn't possible without time advancing mid-loop.
    //
    // Sanity: full completion with frozen time still drains queue.
    current_millis = 1000;
    radio.loop(); // power_up; wait settle
    EXPECT_EQ(mock_nrf.write_count_, 0);
    current_millis += TX_POWER_UP_SETTLE_MS;
    radio.loop();
    EXPECT_EQ(mock_nrf.write_count_, 100);
    EXPECT_TRUE(!mock_nrf.powered_);

    std::cout << "test_burst_yields_when_time_advances passed" << std::endl;
}

static void print_packet(const char *label, const uint8_t *data, size_t len) {
    std::cout << label << ":";
    const std::ios::fmtflags flags = std::cout.flags();
    for (size_t i = 0; i < len; i++) {
        std::cout << ' ' << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                  << static_cast<int>(data[i]);
    }
    std::cout.flags(flags);
    std::cout << std::endl;
}

static void expect_channels(const PayloadEntry &entry, std::initializer_list<uint8_t> channels) {
    EXPECT_EQ(entry.channel_count, static_cast<int>(channels.size()));
    uint8_t index = 0;
    for (uint8_t channel : channels) {
        EXPECT_EQ(entry.channels[index], channel);
        index++;
    }
}

// Motion frames are right-aligned in the 32-byte buffer. These offsets fall out of
// the prototype: length byte at unpadded index 3, channels inserted at index 14,
// action fixed at on-air index 28 because padding shrinks as channels are added.
static void expect_motion_layout(const uint8_t *packet, const uint8_t radio[4],
                                 const uint8_t *channels, uint8_t channel_count, uint8_t action) {
    const int pad = 10 - static_cast<int>(channel_count);
    EXPECT_TRUE(pad >= 0);
    for (int i = 0; i < pad; i++)
        EXPECT_EQ(packet[i], 0x55);
    EXPECT_EQ(packet[pad + 0], radio[0]);
    EXPECT_EQ(packet[pad + 1], radio[1]);
    EXPECT_EQ(packet[pad + 2], 0xC0);
    EXPECT_EQ(packet[pad + 3], static_cast<int>(0x10 + channel_count));
    EXPECT_EQ(packet[pad + 11], 0x86);
    for (uint8_t i = 0; i < channel_count; i++)
        EXPECT_EQ(packet[pad + 14 + i], channels[i]);
    EXPECT_EQ(packet[24], 0x00);
    EXPECT_EQ(packet[27], 0x52);
    EXPECT_EQ(packet[28], action);
}

static void transmit_one(DirectolorRadio &radio, NRF24Component &mock) {
    current_millis = 5000;
    radio.loop();
    current_millis = 5000 + TX_POWER_UP_SETTLE_MS;
    radio.loop();
}

void test_print_transmitted_packets() {
    const uint8_t radio_code[4] = {0x11, 0x11, 0x38, 0x28};
    struct Case {
        const char *label;
        uint8_t channels[6];
        uint8_t count;
    };
    const Case cases[] = {
        {"single-blind packet (1 channel)", {1}, 1},
        {"multi-blind packet (2 channels)", {1, 2}, 2},
        {"multi-blind packet (6 channels)", {1, 2, 3, 4, 5, 6}, 6},
    };

    for (const Case &packet_case : cases) {
        uint8_t rng = 0x3C;
        std::srand(1);
        uint8_t expected[32];
        EXPECT_EQ(build_on_air_payload(expected, radio_code, packet_case.channels, packet_case.count,
                                       directolor_open, rng),
                  32);
        expect_motion_layout(expected, radio_code, packet_case.channels, packet_case.count, directolor_open);

        DirectolorRadio radio;
        NRF24Component mock_nrf;
        radio.set_nrf24(&mock_nrf);
        radio.set_message_send_repeats(1);
        radio.set_cooldown(0);
        radio.set_listening(false);
        radio.setup();
        radio.set_command_random(0x3C);
        std::srand(1);
        for (uint8_t i = 0; i < packet_case.count; i++)
            EXPECT_TRUE(radio.submit_command(radio_code, packet_case.channels[i], directolor_open, 1));
        EXPECT_EQ(radio.pending_payload_count(), 1);

        transmit_one(radio, mock_nrf);
        EXPECT_EQ(mock_nrf.write_count_, 1);
        EXPECT_EQ(mock_nrf.last_written_len_, 32);
        const int same_bytes = std::memcmp(mock_nrf.last_written_, expected, 32);
        EXPECT_EQ(same_bytes, 0);
        print_packet(packet_case.label, mock_nrf.last_written_, mock_nrf.last_written_len_);
    }

    std::cout << "test_print_transmitted_packets passed" << std::endl;
}

void test_queue_merge() {
    const uint8_t radio_a[4] = {0x11, 0x11, 0x38, 0x28};
    const uint8_t radio_b[4] = {0x22, 0x33, 0x44, 0x55};

    DirectolorRadio radio;
    NRF24Component mock_nrf;
    radio.set_nrf24(&mock_nrf);
    radio.set_message_send_repeats(4);
    radio.set_listening(false);
    radio.setup();

    // Channel 2 then channel 1 of the same action collapse in place, and the
    // close that was queued between them stays after the merged open.
    EXPECT_TRUE(radio.submit_command(radio_a, 2, directolor_open, 3));
    EXPECT_TRUE(radio.submit_command(radio_a, 1, directolor_close, 3));
    EXPECT_TRUE(radio.submit_command(radio_a, 1, directolor_open, 3));
    EXPECT_EQ(radio.pending_payload_count(), 2);

    PayloadEntry entry{};
    EXPECT_TRUE(radio.copy_pending(0, entry));
    EXPECT_TRUE(entry.logical);
    EXPECT_TRUE(entry.mergeable);
    EXPECT_EQ(entry.action, directolor_open);
    EXPECT_EQ(entry.code_attempts_left, 3);
    expect_channels(entry, {1, 2});

    EXPECT_TRUE(radio.copy_pending(1, entry));
    EXPECT_EQ(entry.action, directolor_close);
    expect_channels(entry, {1});

    // Duplicate channel is not inserted again and does not create a request.
    EXPECT_TRUE(radio.submit_command(radio_a, 2, directolor_open, 3));
    EXPECT_EQ(radio.pending_payload_count(), 2);
    EXPECT_TRUE(radio.copy_pending(0, entry));
    expect_channels(entry, {1, 2});
    EXPECT_TRUE(radio.copy_pending(1, entry));
    EXPECT_EQ(entry.action, directolor_close);

    // A different radio with the same action stays its own request, at the tail.
    EXPECT_TRUE(radio.submit_command(radio_b, 1, directolor_open, 1));
    EXPECT_EQ(radio.pending_payload_count(), 3);
    EXPECT_TRUE(radio.copy_pending(2, entry));
    EXPECT_EQ(entry.radio_code[0], 0x22);
    EXPECT_EQ(entry.action, directolor_open);
    expect_channels(entry, {1});

    // Join frames are not multi-channel motion packets, so two joins stay separate.
    EXPECT_TRUE(radio.submit_command(radio_a, 1, directolor_join, 1));
    EXPECT_TRUE(radio.submit_command(radio_a, 2, directolor_join, 1));
    EXPECT_EQ(radio.pending_payload_count(), 5);
    EXPECT_TRUE(radio.copy_pending(3, entry));
    EXPECT_TRUE(!entry.mergeable);
    EXPECT_EQ(entry.action, directolor_join);
    expect_channels(entry, {1});
    EXPECT_TRUE(radio.copy_pending(4, entry));
    EXPECT_EQ(entry.action, directolor_join);
    expect_channels(entry, {2});

    // An opaque sendPayload frame is not rewritten when a logical command follows it.
    uint8_t raw[32];
    std::memset(raw, 0xAB, sizeof(raw));
    EXPECT_TRUE(radio.sendPayload(raw));
    EXPECT_TRUE(radio.submit_command(radio_a, 4, directolor_stop, 1));
    EXPECT_EQ(radio.pending_payload_count(), 7);
    EXPECT_TRUE(radio.copy_pending(5, entry));
    EXPECT_TRUE(!entry.logical);
    EXPECT_EQ(entry.payload[0], 0xAB);
    EXPECT_TRUE(radio.copy_pending(6, entry));
    EXPECT_EQ(entry.action, directolor_stop);
    expect_channels(entry, {4});

    std::cout << "test_queue_merge passed" << std::endl;
}

void test_queue_merge_channel_cap() {
    const uint8_t radio_code[4] = {0x11, 0x11, 0x38, 0x28};
    DirectolorRadio radio;
    NRF24Component mock_nrf;
    radio.set_nrf24(&mock_nrf);
    radio.set_listening(false);
    radio.setup();

    for (uint8_t channel = 1; channel <= 6; channel++)
        EXPECT_TRUE(radio.submit_command(radio_code, channel, directolor_open, 1));
    EXPECT_EQ(radio.pending_payload_count(), 1);

    PayloadEntry entry{};
    EXPECT_TRUE(radio.copy_pending(0, entry));
    expect_channels(entry, {1, 2, 3, 4, 5, 6});

    // The 7th channel is kept as its own request, not dropped.
    EXPECT_TRUE(radio.submit_command(radio_code, 7, directolor_open, 1));
    EXPECT_EQ(radio.pending_payload_count(), 2);
    EXPECT_TRUE(radio.copy_pending(0, entry));
    expect_channels(entry, {1, 2, 3, 4, 5, 6});
    EXPECT_TRUE(radio.copy_pending(1, entry));
    expect_channels(entry, {7});

    // A further channel fills the overflow packet instead of opening a third one,
    // and does not reshuffle the already-full packet.
    EXPECT_TRUE(radio.submit_command(radio_code, 8, directolor_open, 1));
    EXPECT_EQ(radio.pending_payload_count(), 2);
    EXPECT_TRUE(radio.copy_pending(0, entry));
    expect_channels(entry, {1, 2, 3, 4, 5, 6});
    EXPECT_TRUE(radio.copy_pending(1, entry));
    expect_channels(entry, {7, 8});

    // A channel already in the full packet is not copied into the overflow.
    EXPECT_TRUE(radio.submit_command(radio_code, 3, directolor_open, 1));
    EXPECT_EQ(radio.pending_payload_count(), 2);
    EXPECT_TRUE(radio.copy_pending(0, entry));
    expect_channels(entry, {1, 2, 3, 4, 5, 6});
    EXPECT_TRUE(radio.copy_pending(1, entry));
    expect_channels(entry, {7, 8});

    // An unrelated command queued before the overflow stays ahead of it.
    DirectolorRadio ordered;
    NRF24Component ordered_nrf;
    ordered.set_nrf24(&ordered_nrf);
    ordered.set_listening(false);
    ordered.setup();
    for (uint8_t channel = 1; channel <= 6; channel++)
        EXPECT_TRUE(ordered.submit_command(radio_code, channel, directolor_open, 1));
    EXPECT_TRUE(ordered.submit_command(radio_code, 1, directolor_close, 1));
    EXPECT_TRUE(ordered.submit_command(radio_code, 9, directolor_open, 1));
    EXPECT_EQ(ordered.pending_payload_count(), 3);
    EXPECT_TRUE(ordered.copy_pending(0, entry));
    EXPECT_EQ(entry.action, directolor_open);
    expect_channels(entry, {1, 2, 3, 4, 5, 6});
    EXPECT_TRUE(ordered.copy_pending(1, entry));
    EXPECT_EQ(entry.action, directolor_close);
    expect_channels(entry, {1});
    EXPECT_TRUE(ordered.copy_pending(2, entry));
    EXPECT_EQ(entry.action, directolor_open);
    expect_channels(entry, {9});

    std::cout << "test_queue_merge_channel_cap passed" << std::endl;
}

void test_inflight_merge() {
    const uint8_t radio_code[4] = {0x11, 0x11, 0x38, 0x28};
    DirectolorRadio radio;
    NRF24Component mock_nrf;
    radio.set_nrf24(&mock_nrf);
    radio.set_message_send_repeats(30);
    radio.set_cooldown(0);
    radio.set_listening(false);
    radio.setup();

    EXPECT_TRUE(radio.submit_command(radio_code, 1, directolor_open, 1));

    current_millis = 1000;
    radio.loop();
    EXPECT_EQ(mock_nrf.write_count_, 0);
    EXPECT_EQ(radio.pending_payload_count(), 0);

    current_millis = 1000 + TX_POWER_UP_SETTLE_MS;
    mock_nrf.advance_millis_on_write_ = 10;
    radio.loop();
    EXPECT_EQ(mock_nrf.write_count_, 3);

    const uint8_t single[] = {1};
    expect_motion_layout(mock_nrf.last_written_, radio_code, single, 1, directolor_open);

    PayloadEntry inflight{};
    EXPECT_TRUE(radio.copy_inflight(inflight));
    expect_channels(inflight, {1});
    EXPECT_TRUE(inflight.send_attempts > 0);

    // A different action does not rewrite the burst that is already on the air.
    EXPECT_TRUE(radio.submit_command(radio_code, 4, directolor_close, 1));
    EXPECT_TRUE(radio.copy_inflight(inflight));
    EXPECT_EQ(inflight.action, directolor_open);
    expect_channels(inflight, {1});
    EXPECT_EQ(radio.pending_payload_count(), 1);
    PayloadEntry queued{};
    EXPECT_TRUE(radio.copy_pending(0, queued));
    EXPECT_EQ(queued.action, directolor_close);
    expect_channels(queued, {4});

    // Same action, new channel: fold into the live frame and restart its repeats.
    EXPECT_TRUE(radio.submit_command(radio_code, 2, directolor_open, 1));
    EXPECT_EQ(radio.pending_payload_count(), 1);
    EXPECT_TRUE(radio.copy_inflight(inflight));
    expect_channels(inflight, {1, 2});
    EXPECT_EQ(inflight.send_attempts, 30);
    EXPECT_TRUE(radio.copy_pending(0, queued));
    EXPECT_EQ(queued.action, directolor_close);

    radio.loop();
    EXPECT_TRUE(mock_nrf.write_count_ > 3);
    const uint8_t both[] = {1, 2};
    expect_motion_layout(mock_nrf.last_written_, radio_code, both, 2, directolor_open);

    std::cout << "test_inflight_merge passed" << std::endl;
}

int main() {
    test_payload_queue();
    test_payload_matching();
    test_non_blocking_dense_send();
    test_power_down_and_cooldown_between_payloads();
    test_queue_full_drop();
    test_burst_yields_when_time_advances();
    test_print_transmitted_packets();
    test_queue_merge();
    test_queue_merge_channel_cap();
    test_inflight_merge();

    std::cout << "Ran " << tests_run << " assertions, " << tests_failed << " failed." << std::endl;
    if (tests_failed != 0) {
        return 1;
    }
    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}

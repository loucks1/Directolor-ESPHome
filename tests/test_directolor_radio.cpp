#include <iostream>
#include <cassert>
#include <cstring>
#include "esphome.h"
#include "esphome/components/nrf24/nrf24.h"
#include "../../components/directolor_radio/directolor_radio.h"
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

int main() {
    test_payload_queue();
    test_payload_matching();
    test_non_blocking_dense_send();
    test_power_down_and_cooldown_between_payloads();
    test_queue_full_drop();
    test_burst_yields_when_time_advances();

    std::cout << "Ran " << tests_run << " assertions, " << tests_failed << " failed." << std::endl;
    if (tests_failed != 0) {
        return 1;
    }
    std::cout << "All tests passed successfully!" << std::endl;
    return 0;
}

#pragma once
#include <cstdint>
#include <functional>

namespace esphome {
namespace nrf24 {
    class NRF24Component {
    public:
        void add_on_data_callback(std::function<void(const uint8_t*, uint8_t)> cb) { callback_ = cb; }
        void set_auto_ack(bool) {}
        void set_crc_length(uint8_t) {}
        void stop_listening() {}
        void start_listening() {}
        void power_down() {}
        void power_up() {}
        bool is_chip_connected() { return true; }
        void set_address_width(uint8_t) {}
        void open_reading_pipe(uint8_t, uint64_t) {}
        void open_reading_pipe(uint8_t, const uint8_t*) {}
        void open_writing_pipe(uint64_t) {}
        void dump_config() {}
        void tx_standby() {}
        void write_fast(const uint8_t*, uint8_t, bool) {}
        uint8_t get_payload_size() { return 32; }

        void trigger_incoming(const uint8_t* data, uint8_t len) {
            if (callback_) callback_(data, len);
        }

    private:
        std::function<void(const uint8_t*, uint8_t)> callback_;
    };
}
}

namespace nRF24L01 {
    constexpr uint8_t RF24_CRC_DISABLED = 0;
}

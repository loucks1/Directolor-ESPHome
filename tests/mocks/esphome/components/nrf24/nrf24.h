#pragma once
#include <cstdint>
#include <cstring>
#include <functional>

// Defined by the unit-test harness. write_fast can advance it so a burst yields.
extern unsigned long current_millis;

namespace esphome {
namespace nrf24 {
    class NRF24Component {
    public:
        void add_on_data_callback(std::function<void(const uint8_t*, uint8_t)> cb) { callback_ = cb; }
        void set_auto_ack(bool) {}
        void set_crc_length(uint8_t) {}
        void stop_listening() { listening_ = false; }
        void start_listening() { listening_ = true; }
        void power_down() { powered_ = false; power_down_count_++; }
        void power_up() { powered_ = true; power_up_count_++; }
        bool is_chip_connected() { return true; }
        void set_address_width(uint8_t w) { address_width_ = w; }
        void open_reading_pipe(uint8_t, uint64_t) {}
        void open_reading_pipe(uint8_t, const uint8_t*) {}
        void open_writing_pipe(uint64_t) { writing_ = true; }
        void dump_config() {}
        void tx_standby() { standby_count_++; }
        void write_fast(const uint8_t *data, uint8_t len, bool)
        {
            write_count_++;
            last_written_len_ = len > sizeof(last_written_) ? static_cast<uint8_t>(sizeof(last_written_)) : len;
            if (data != nullptr && last_written_len_ > 0)
                std::memcpy(last_written_, data, last_written_len_);
            if (advance_millis_on_write_ > 0)
                current_millis += static_cast<unsigned long>(advance_millis_on_write_);
        }
        uint8_t get_payload_size() { return 32; }

        void trigger_incoming(const uint8_t* data, uint8_t len) {
            if (callback_) callback_(data, len);
        }

        int write_count_ = 0;
        int standby_count_ = 0;
        int power_down_count_ = 0;
        int power_up_count_ = 0;
        bool listening_ = false;
        bool powered_ = true;
        bool writing_ = false;
        uint8_t address_width_ = 5;
        uint8_t last_written_[32]{};
        uint8_t last_written_len_{0};
        int advance_millis_on_write_{0};

    private:
        std::function<void(const uint8_t*, uint8_t)> callback_;
    };
}
}

namespace nRF24L01 {
    constexpr uint8_t RF24_CRC_DISABLED = 0;
}

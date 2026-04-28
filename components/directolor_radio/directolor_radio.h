#pragma once

#include "esphome/components/directolor_radio/payload_queue.h"
#include "esphome/components/directolor_radio/directolor_radio_types.h"
#include "esphome/components/nrf24/nrf24.h"

namespace esphome
{
    namespace directolor_radio
    {
        enum RemoteLearnState
        {
            REMOTE_STATE_NOT_STARTED = 0,
            REMOTE_STATE_LEARNING = 1,
            REMOTE_STATE_CAPTURED = 2,
        };

        class DirectolorRadio : public Component
        {
        public:
            void set_nrf24(nrf24::NRF24Component *parent) { this->radio_ = parent; }
            void set_directolor_code_attempts(uint8_t attempts) { this->code_attempts_ = attempts; }
            void set_message_send_repeats(uint16_t repeats) { this->message_send_repeats_ = repeats; }

            void setup() override;
            void loop() override;
            void dump_config() override;

            // Made this public so the Cover component can feed the radio
            void sendPayload(uint8_t *payload);
            uint8_t get_code_attempts() const { return this->code_attempts_; }

            void set_listening(bool listening) { this->listening_ = listening; }
            bool is_listening() const { return listening_; }

        protected:
            nrf24::NRF24Component *radio_;

            bool radioStarted();
            bool enterRemoteSearchMode();
            void process_incoming_packet(const uint8_t *data, uint8_t len);
            void enterRemoteCaptureMode();
            void send_code();

            bool listening_{true};

            RemoteLearnState CaptureState_{REMOTE_STATE_NOT_STARTED};

            uint32_t lastStartAttempt{0};
            uint32_t cooldown_{20};
            unsigned long lastMillis_ = 0;

            uint8_t code_attempts_;
            uint16_t message_send_repeats_;

            // This matches your 4-byte remote code logic
            std::array<uint8_t, 4> sniffed_remote_code_{{0x00, 0x00, 0x00, 0x00}};

            PayloadQueue queue_;
            PayloadEntry current_sending_payload_;
        };

    } // namespace directolor_radio
} // namespace esphome
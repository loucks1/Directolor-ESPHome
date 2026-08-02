#pragma once

#include "esphome/components/directolor_radio/payload_queue.h"
#include "esphome/components/directolor_radio/directolor_radio_types.h"
#include "esphome/components/nrf24/nrf24.h"

namespace esphome
{
    namespace directolor_radio
    {
        /**
         * @brief States for remote sniffing and learning
         */
        enum RemoteLearnState
        {
            REMOTE_STATE_NOT_STARTED = 0, ///< Normal operation, not sniffing
            REMOTE_STATE_LEARNING = 1,    ///< Listening for broad patterns to find a remote
            REMOTE_STATE_CAPTURED = 2,    ///< Remote pattern found, waiting for full payload
        };

        /**
         * @brief Core component for managing NRF24 communication for Directolor shades
         */
        class DirectolorRadio : public Component
        {
        public:
            /** @brief Sets the parent NRF24 radio component */
            void set_nrf24(nrf24::NRF24Component *parent) { this->radio_ = parent; }
            /** @brief Sets the number of times to attempt sending a code */
            void set_directolor_code_attempts(uint8_t attempts) { this->code_attempts_ = attempts; }
            /** @brief Sets the number of packet repeats per send attempt */
            void set_message_send_repeats(uint16_t repeats) { this->message_send_repeats_ = repeats; }
            /** @brief Sets the cooldown time (ms) between send attempts */
            void set_cooldown(uint8_t cooldown) { this->cooldown_ = cooldown; }

            /** @brief ESPHome component setup phase */
            void setup() override;
            /** @brief ESPHome component loop phase (non-blocking) */
            void loop() override;
            /** @brief Dumps configuration to log */
            void dump_config() override;

            /**
             * @brief Enqueues a shade command payload to be sent over the radio
             * @param payload 32-byte payload to send
             */
            void sendPayload(uint8_t *payload);
            uint8_t get_code_attempts() const { return this->code_attempts_; }

            /** @brief Enables or disables RX listening mode */
            void set_listening(bool listening) { this->listening_ = listening; }
            bool is_listening() const { return listening_; }

            /**
             * @brief Converts a BlindAction enum to a human-readable string
             */
            const char *blind_action_to_string(BlindAction action);

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
            uint8_t cooldown_{30};
            unsigned long lastMillis_ = 0;
            unsigned long lastSendAttemptMillis_ = 0;
            unsigned long tx_standby_start_ = 0;
            bool tx_is_standby_ = false;


            uint8_t code_attempts_;
            uint16_t message_send_repeats_;

            // This matches your 4-byte remote code logic
            std::array<uint8_t, 4> sniffed_remote_code_{{0x00, 0x00, 0x00, 0x00}};
            bool has_sniffed_code_ = false;


            PayloadQueue queue_;
            PayloadEntry current_sending_payload_;
        };

    } // namespace directolor_radio
} // namespace esphome
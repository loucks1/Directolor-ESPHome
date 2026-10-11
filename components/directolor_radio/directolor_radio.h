#pragma once

#include "esphome/core/component.h"
#include "esphome/components/directolor_radio/payload_queue.h"
#include "esphome/components/directolor_radio/directolor_radio_types.h"
#include "esphome/components/nrf24/nrf24.h"
#include <array>
#include <cstdint>

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
            /** @brief Sets the cooldown time (ms) between distinct queued payloads */
            void set_cooldown(uint16_t cooldown) { this->cooldown_ = cooldown; }

            /** @brief ESPHome component setup phase */
            void setup() override;
            /** @brief ESPHome component loop phase (non-blocking) */
            void loop() override;
            /** @brief Dumps configuration to log */
            void dump_config() override;

            /**
             * @brief Enqueues a shade command payload to be sent over the radio
             * @param payload 32-byte payload to send
             * @return false if the queue is full and the payload was dropped
             */
            bool sendPayload(const uint8_t *payload);

            /**
             * @brief Queue a shade command.
             *
             * Mergeable actions (open/close/stop/tilt/to-favorite) for the same
             * 4-byte radio are combined into one frame of up to REMOTE_CHANNELS
             * channels. A matching in-flight or queued command is updated in place.
             * A 7th distinct channel is kept as a following request. Duplicate
             * channels are not added twice. Other actions are queued as their own
             * requests and do not disturb unrelated entries.
             *
             * @param code_attempts Distinct random/CRC payloads to transmit for this command
             * @return false if a new queue slot was required and the queue was full
             */
            bool submit_command(const uint8_t radio_code[4], uint8_t channel, BlindAction action, uint8_t code_attempts);

            uint8_t get_code_attempts() const { return this->code_attempts_; }

            /** @brief Copy a queued request. Index 0 is the next one to transmit. */
            bool copy_pending(uint8_t index, PayloadEntry &out) const { return this->queue_.copy_at(index, out); }
            /** @brief Copy the command currently being repeated, if one is active. */
            bool copy_inflight(PayloadEntry &out) const
            {
                if (!this->current_active_)
                    return false;
                out = this->current_sending_payload_;
                return true;
            }
            void set_command_random(uint8_t value) { this->command_random_ = value; }

            /** @brief Enables or disables RX listening mode */
            void set_listening(bool listening) { this->listening_ = listening; }
            bool is_listening() const { return listening_; }

            /** @brief Whether a remote address has been sniffed (diagnostics / tests) */
            bool has_sniffed_code() const { return this->has_sniffed_code_; }
            /** @brief Last sniffed 4-byte remote address */
            std::array<uint8_t, 4> get_sniffed_remote_code() const { return this->sniffed_remote_code_; }
            /** @brief Number of payloads waiting to be transmitted */
            uint8_t pending_payload_count() const { return this->queue_.size(); }

            /**
             * @brief Converts a BlindAction enum to a human-readable string
             */
            const char *blind_action_to_string(BlindAction action);

        protected:
            nrf24::NRF24Component *radio_{nullptr};

            bool enterRemoteSearchMode();
            void process_incoming_packet(const uint8_t *data, uint8_t len);
            void enterRemoteCaptureMode();
            void send_code();
            bool enqueue_logical_(const uint8_t radio_code[4], const uint8_t *channels, uint8_t channel_count,
                                  BlindAction action, uint8_t code_attempts, bool mergeable);
            bool merge_motion_(const uint8_t radio_code[4], uint8_t channel, BlindAction action, uint8_t code_attempts);
            bool prepare_current_payload_();
            void restart_inflight_(PayloadEntry &entry, uint8_t code_attempts);

            bool listening_{true};
            bool continue_current_{false};
            bool current_active_{false};
            uint8_t command_random_{0};

            RemoteLearnState CaptureState_{REMOTE_STATE_NOT_STARTED};

            uint16_t cooldown_{100};
            unsigned long last_rx_millis_{0};
            uint8_t last_rx_action_{0xFF};
            unsigned long last_payload_finish_ms_{0};
            unsigned long radio_power_up_ms_{0};

            uint8_t code_attempts_{3};
            uint16_t message_send_repeats_{513};

            // This matches your 4-byte remote code logic
            std::array<uint8_t, 4> sniffed_remote_code_{{0x00, 0x00, 0x00, 0x00}};
            bool has_sniffed_code_{false};

            PayloadQueue queue_;
            PayloadEntry current_sending_payload_{};
        };

    } // namespace directolor_radio
} // namespace esphome

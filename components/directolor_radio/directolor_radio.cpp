#include "directolor_radio.h"
#include "directolor_packet.h"

#include <esphome/core/log.h>
#include "esphome.h"
#include "esp_random.h"
#include <string>
#include <cstring>

namespace esphome
{
    namespace directolor_radio
    {
        static const char *TAG = "directolor_radio";
        static const uint8_t MATCHPATTERN[4] = {0xC0, 0X11, 0X00, 0X05}; // this is what we use to find out the codes for a new remote

        /**
         * @brief Component setup, registers radio data callback and configures CRC
         */
        void DirectolorRadio::setup()
        {
            this->current_sending_payload_.send_attempts = 0;
            this->current_active_ = false;
            this->continue_current_ = false;
            this->command_random_ = static_cast<uint8_t>(esp_random() % 256);

            this->radio_->add_on_data_callback([this](const uint8_t *data, uint8_t len)
                                               { this->process_incoming_packet(data, len); });
            this->radio_->set_auto_ack(false);
            this->radio_->set_crc_length(nRF24L01::RF24_CRC_DISABLED);
        }

        /**
         * @brief Component non-blocking loop, handles state transitions and sending code
         */
        void DirectolorRadio::loop()
        {
            if (this->listening_ && this->CaptureState_ == REMOTE_STATE_NOT_STARTED)
            {
                this->enterRemoteSearchMode();
                return;
            }

            if (!this->listening_ && this->CaptureState_ != REMOTE_STATE_NOT_STARTED)
            {
                this->radio_->stop_listening();
                this->radio_->power_down();
                this->CaptureState_ = REMOTE_STATE_NOT_STARTED;
                ESP_LOGI(TAG, "stopped listening - will not capture remote codes until re-enabled");
                return;
            }

            this->send_code();
        }

        const char *DirectolorRadio::blind_action_to_string(BlindAction action)
        {
            switch (action)
            {
            case directolor_open:
                return "Open";
            case directolor_close:
                return "Close";
            case directolor_stop:
                return "Stop";
            case directolor_join:
                return "Join";
            case directolor_remove:
                return "Remove";
            case directolor_duplicate:
                return "Duplicate";
            case directolor_setFav:
                return "Set Favorite";
            case directolor_tiltOpen:
                return "Tilt Open";
            case directolor_tiltClose:
                return "Tilt Close";
            case directolor_toFav:
                return "To Favorite";
            default:
                return "unknown";
            }
        }

        /**
         * @brief Processes an incoming payload received from the NRF24 radio.
         * Handling either learning mode sniffing or normal operation command receiving.
         * @param payload Pointer to the received data
         * @param bytes Length of the received data
         */
        void DirectolorRadio::process_incoming_packet(const uint8_t *payload, uint8_t bytes)
        {
            // 1. Sniffing / Learning Mode
            if (this->CaptureState_ == REMOTE_STATE_LEARNING)
            {
                ESP_LOGV(TAG, "Checking payload for match: %s", format_hex_pretty(payload, bytes).c_str());

                for (int idx = 2; idx <= bytes - 9; idx++)
                {
                    if (memcmp(&payload[idx], MATCHPATTERN, 4) == 0)
                    {
                        this->sniffed_remote_code_[0] = payload[idx - 2];
                        this->sniffed_remote_code_[1] = payload[idx - 1];
                        this->sniffed_remote_code_[2] = payload[idx + 7];
                        this->sniffed_remote_code_[3] = payload[idx + 8];

                        this->has_sniffed_code_ = true;
                        this->CaptureState_ = REMOTE_STATE_CAPTURED;

                        ESP_LOGI(TAG, "Found Remote with address: [0x%02X, 0x%02X, 0x%02X, 0x%02X]",
                                 this->sniffed_remote_code_[0], this->sniffed_remote_code_[1],
                                 this->sniffed_remote_code_[2], this->sniffed_remote_code_[3]);

                        this->enterRemoteCaptureMode();
                        return; // Exit early once captured
                    }
                }
                return;
            }

            // 2. Normal Operation / Processing Commands
            // Basic validation based on your protocol's first byte
            uint8_t expected_bytes = payload[0] + 4;
            if (expected_bytes > esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE || expected_bytes > bytes)
            {
                return; // Invalid or incomplete payload
            }

            const char *command = "ERROR";
            uint8_t action_byte = 0xFF;

            // Standard Command Check (0x86)
            if (payload[4] == 0xFF && payload[5] == 0xFF &&
                payload[6] == this->sniffed_remote_code_[2] &&
                payload[7] == this->sniffed_remote_code_[3] && payload[8] == 0x86)
            {
                action_byte = payload[expected_bytes - 5];
                command = blind_action_to_string(static_cast<BlindAction>(action_byte));
            }

            // Join/Remove Check (0x08)
            if (payload[4] == 0xFF && payload[5] == 0xFF &&
                payload[6] == this->sniffed_remote_code_[2] &&
                payload[7] == this->sniffed_remote_code_[3] && payload[8] == 0x08)
            {
                action_byte = payload[expected_bytes - 4];
                command = blind_action_to_string(static_cast<BlindAction>(action_byte));
            }

            // Duplicate Check (0xC8)
            if (payload[0] == 0x12 && payload[1] == 0x80 && payload[2] == 0x0D &&
                payload[4] == 0xFF && payload[5] == 0xFF &&
                payload[16] == this->sniffed_remote_code_[2] &&
                payload[17] == this->sniffed_remote_code_[3] && payload[18] == 0xC8)
            {
                action_byte = directolor_duplicate;
                command = "Duplicate";
            }

            if (strcmp(command, "ERROR") == 0)
            {
                return;
            }

            // Debounce only identical repeated actions (physical remotes blast many copies)
            uint32_t now = millis();
            if (action_byte == this->last_rx_action_ && (now - this->last_rx_millis_ < RX_DEBOUNCE_MS))
            {
                return;
            }
            this->last_rx_millis_ = now;
            this->last_rx_action_ = action_byte;

            // Log success
            char addr_buffer[12];
            ESP_LOGI(TAG, "Received %s from: %s", command,
                     format_hex_pretty_to(addr_buffer, this->sniffed_remote_code_.data(), 4));
        }

        void DirectolorRadio::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Directolor Radio:");
            ESP_LOGCONFIG(TAG, "  Code Attempts: %d", this->code_attempts_);
            ESP_LOGCONFIG(TAG, "  Message Send Repeats: %d", this->message_send_repeats_);
            ESP_LOGCONFIG(TAG, "  Inter-payload Cooldown: %d ms", this->cooldown_);
        }

        /**
         * @brief Transitions the radio into sniffing mode, searching for a 4-byte pattern
         * @return true if successfully entered learning mode
         */
        bool DirectolorRadio::enterRemoteSearchMode()
        {
            if (this->radio_->is_chip_connected())
            {
                ESP_LOGI(TAG, "attempting to start listening");
                this->radio_->stop_listening();
                this->radio_->set_address_width(5);
                this->radio_->open_reading_pipe(1, 0x5555555555); // always uses pipe 0
                ESP_LOGI(TAG, "searching for remote");
                this->radio_->start_listening(); // put radio in RX mode
                this->CaptureState_ = REMOTE_STATE_LEARNING;
                ESP_LOGI(TAG, "started listening - press a button on your remote");
                if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE)
                    this->radio_->dump_config();
                return true;
            }
            return false;
        }

        void DirectolorRadio::enterRemoteCaptureMode()
        {
            if (!this->listening_)
            {
                this->CaptureState_ = REMOTE_STATE_NOT_STARTED;
                this->radio_->stop_listening();
                this->radio_->power_down();
                return;
            }
            if (this->has_sniffed_code_)
            {
                this->radio_->stop_listening();
                this->radio_->set_address_width(3);

                uint8_t capture_addr[3];
                capture_addr[2] = this->sniffed_remote_code_[0];
                capture_addr[1] = this->sniffed_remote_code_[1];
                capture_addr[0] = 0xC0;
                this->radio_->open_reading_pipe(1, capture_addr);

                this->radio_->start_listening(); // put radio in RX mode
                this->CaptureState_ = REMOTE_STATE_CAPTURED;
                ESP_LOGI(TAG, "Now listening for 3-byte payloads on address: [%02X, %02X, %02X]",
                         capture_addr[0], capture_addr[1], capture_addr[2]);
            }
            else if (this->CaptureState_ == REMOTE_STATE_LEARNING)
            {
                enterRemoteSearchMode();
            }
            else
            {
                this->radio_->power_down();
            }
        }

        bool DirectolorRadio::sendPayload(const uint8_t *payload)
        {
            if (!this->queue_.enqueue(payload, this->message_send_repeats_))
            {
                ESP_LOGW(TAG, "Payload queue full (capacity=%u); dropping command",
                         static_cast<unsigned>(PayloadQueue::capacity()));
                return false;
            }
            return true;
        }

        bool DirectolorRadio::enqueue_logical_(const uint8_t radio_code[4], const uint8_t *channels, uint8_t channel_count,
                                               BlindAction action, uint8_t code_attempts, bool mergeable)
        {
            if (channel_count > REMOTE_CHANNELS)
                channel_count = REMOTE_CHANNELS;

            PayloadEntry entry{};
            entry.logical = true;
            entry.mergeable = mergeable;
            entry.payload_ready = false;
            entry.send_attempts = 0;
            entry.action = action;
            entry.channel_count = channel_count;
            entry.code_attempts_left = code_attempts == 0 ? 1 : code_attempts;
            std::memcpy(entry.radio_code, radio_code, 4);
            if (channels != nullptr && channel_count > 0)
                std::memcpy(entry.channels, channels, channel_count);

            if (!this->queue_.enqueue_entry(entry))
            {
                ESP_LOGW(TAG, "Payload queue full (capacity=%u); dropping command",
                         static_cast<unsigned>(PayloadQueue::capacity()));
                return false;
            }
            return true;
        }

        bool DirectolorRadio::prepare_current_payload_()
        {
            PayloadEntry &entry = this->current_sending_payload_;
            if (!entry.logical)
                return entry.send_attempts > 0;

            const int built = build_on_air_payload(entry.payload, entry.radio_code, entry.channels, entry.channel_count,
                                                   entry.action, this->command_random_);
            if (built <= 0)
            {
                ESP_LOGE(TAG, "Failed to build radio payload");
                entry.send_attempts = 0;
                entry.payload_ready = false;
                return false;
            }
            entry.payload_ready = true;
            entry.send_attempts = this->message_send_repeats_;
            return true;
        }

        void DirectolorRadio::restart_inflight_(PayloadEntry &entry, uint8_t code_attempts)
        {
            entry.code_attempts_left = code_attempts == 0 ? 1 : code_attempts;
            entry.payload_ready = false;
            // A burst is already running (or waiting out oscillator settle). Rebuild now
            // and restart the repeat count so the combined frame is what goes on the air.
            if (entry.send_attempts > 0)
            {
                const int built = build_on_air_payload(entry.payload, entry.radio_code, entry.channels, entry.channel_count,
                                                       entry.action, this->command_random_);
                if (built <= 0)
                {
                    entry.send_attempts = 0;
                    this->current_active_ = false;
                    this->continue_current_ = false;
                    return;
                }
                entry.payload_ready = true;
                entry.send_attempts = this->message_send_repeats_;
                return;
            }

            // Between distinct code attempts the radio is powered down for cooldown.
            // Keep the command current; the next acquire builds the updated frame.
            this->continue_current_ = true;
            this->current_active_ = true;
        }

        namespace
        {
            bool insert_channel_sorted(uint8_t *channels, uint8_t &count, uint8_t channel, uint8_t cap)
            {
                for (uint8_t i = 0; i < count; i++)
                {
                    if (channels[i] == channel)
                        return false;
                    if (channels[i] > channel)
                    {
                        if (count >= cap)
                            return false;
                        for (uint8_t j = count; j > i; j--)
                            channels[j] = channels[j - 1];
                        channels[i] = channel;
                        count++;
                        return true;
                    }
                }
                if (count >= cap)
                    return false;
                channels[count++] = channel;
                return true;
            }

            bool same_channels(const PayloadEntry &entry, const uint8_t *channels, uint8_t count)
            {
                if (entry.channel_count != count)
                    return false;
                return count == 0 || std::memcmp(entry.channels, channels, count) == 0;
            }

            bool entry_has_channel(const PayloadEntry &entry, uint8_t channel)
            {
                for (uint8_t i = 0; i < entry.channel_count; i++)
                {
                    if (entry.channels[i] == channel)
                        return true;
                }
                return false;
            }
        } // namespace

        bool DirectolorRadio::merge_motion_(const uint8_t radio_code[4], uint8_t channel, BlindAction action,
                                            uint8_t code_attempts)
        {
            if (code_attempts == 0)
                code_attempts = 1;

            struct Slot
            {
                bool in_flight;
                uint8_t queue_index;
            };

            Slot slots[PAYLOAD_QUEUE_SIZE + 1];
            uint8_t slot_count = 0;

            auto matches = [&](const PayloadEntry &entry) {
                return entry.logical && entry.mergeable && entry.action == action &&
                       std::memcmp(entry.radio_code, radio_code, 4) == 0;
            };
            auto load_slot = [&](const Slot &slot, PayloadEntry &out) {
                if (slot.in_flight)
                {
                    out = this->current_sending_payload_;
                    return this->current_active_;
                }
                return this->queue_.copy_at(slot.queue_index, out);
            };
            auto store_slot = [&](const Slot &slot, const PayloadEntry &entry) {
                if (slot.in_flight)
                    this->current_sending_payload_ = entry;
                else
                    this->queue_.update_at(slot.queue_index, entry);
            };

            if (this->current_active_ && matches(this->current_sending_payload_))
            {
                slots[slot_count].in_flight = true;
                slots[slot_count].queue_index = 0;
                slot_count++;
            }
            for (uint8_t i = 0; i < this->queue_.size(); i++)
            {
                PayloadEntry queued{};
                if (!this->queue_.copy_at(i, queued) || !matches(queued))
                    continue;
                slots[slot_count].in_flight = false;
                slots[slot_count].queue_index = i;
                slot_count++;
            }

            uint8_t all[PAYLOAD_QUEUE_SIZE * REMOTE_CHANNELS + 1];
            uint8_t all_count = 0;
            for (uint8_t s = 0; s < slot_count; s++)
            {
                PayloadEntry entry{};
                if (!load_slot(slots[s], entry))
                    continue;
                for (uint8_t c = 0; c < entry.channel_count; c++)
                    insert_channel_sorted(all, all_count, entry.channels[c], sizeof(all));
            }

            const bool added = insert_channel_sorted(all, all_count, channel, sizeof(all));
            if (!added)
            {
                // Same channel is already in a matching request. Restart that request so
                // a repeated press is still transmitted, without duplicating the channel.
                for (uint8_t s = 0; s < slot_count; s++)
                {
                    PayloadEntry entry{};
                    if (!load_slot(slots[s], entry) || !entry_has_channel(entry, channel))
                        continue;
                    entry.code_attempts_left = code_attempts;
                    entry.payload_ready = false;
                    if (slots[s].in_flight)
                        this->restart_inflight_(entry, code_attempts);
                    store_slot(slots[s], entry);
                    return true;
                }
                return true;
            }

            const uint8_t packets_needed =
                static_cast<uint8_t>((all_count + REMOTE_CHANNELS - 1) / REMOTE_CHANNELS);
            const uint8_t new_slots = packets_needed > slot_count ? static_cast<uint8_t>(packets_needed - slot_count) : 0;
            if (static_cast<size_t>(this->queue_.size()) + new_slots > PayloadQueue::capacity())
            {
                ESP_LOGW(TAG, "Payload queue full (capacity=%u); dropping command",
                         static_cast<unsigned>(PayloadQueue::capacity()));
                return false;
            }

            for (uint8_t p = 0; p < packets_needed && p < slot_count; p++)
            {
                const uint8_t begin = static_cast<uint8_t>(p * REMOTE_CHANNELS);
                const uint8_t count = static_cast<uint8_t>(
                    (all_count - begin) > REMOTE_CHANNELS ? REMOTE_CHANNELS : (all_count - begin));
                PayloadEntry entry{};
                if (!load_slot(slots[p], entry))
                    continue;
                if (same_channels(entry, all + begin, count))
                    continue;

                std::memcpy(entry.channels, all + begin, count);
                if (count < REMOTE_CHANNELS)
                    std::memset(entry.channels + count, 0, REMOTE_CHANNELS - count);
                entry.channel_count = count;
                entry.payload_ready = false;
                entry.code_attempts_left = code_attempts;
                if (slots[p].in_flight)
                    this->restart_inflight_(entry, code_attempts);
                store_slot(slots[p], entry);
            }

            if (slot_count > packets_needed)
            {
                uint8_t remove_idx[PAYLOAD_QUEUE_SIZE];
                uint8_t remove_n = 0;
                for (uint8_t s = packets_needed; s < slot_count; s++)
                {
                    if (!slots[s].in_flight)
                        remove_idx[remove_n++] = slots[s].queue_index;
                }
                for (uint8_t i = 0; i < remove_n; i++)
                {
                    for (uint8_t j = static_cast<uint8_t>(i + 1); j < remove_n; j++)
                    {
                        if (remove_idx[j] > remove_idx[i])
                        {
                            const uint8_t tmp = remove_idx[i];
                            remove_idx[i] = remove_idx[j];
                            remove_idx[j] = tmp;
                        }
                    }
                }
                for (uint8_t i = 0; i < remove_n; i++)
                    this->queue_.remove_at(remove_idx[i]);
            }

            for (uint8_t p = slot_count; p < packets_needed; p++)
            {
                const uint8_t begin = static_cast<uint8_t>(p * REMOTE_CHANNELS);
                const uint8_t count = static_cast<uint8_t>(
                    (all_count - begin) > REMOTE_CHANNELS ? REMOTE_CHANNELS : (all_count - begin));
                if (!this->enqueue_logical_(radio_code, all + begin, count, action, code_attempts, true))
                    return false;
            }

            ESP_LOGD(TAG, "Combined %s into %u channel(s) across %u packet(s)",
                     this->blind_action_to_string(action), static_cast<unsigned>(all_count),
                     static_cast<unsigned>(packets_needed));
            return true;
        }

        bool DirectolorRadio::submit_command(const uint8_t radio_code[4], uint8_t channel, BlindAction action,
                                             uint8_t code_attempts)
        {
            if (radio_code == nullptr)
                return false;
            if (code_attempts == 0)
                code_attempts = 1;

            if (!action_is_mergeable(action))
            {
                uint8_t channels[1] = {channel};
                const uint8_t count =
                    (action == directolor_join || action == directolor_remove) ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0);
                return this->enqueue_logical_(radio_code, channels, count, action, code_attempts, false);
            }
            return this->merge_motion_(radio_code, channel, action, code_attempts);
        }

        /**
         * @brief Non-blocking state machine for handling command transmissions.
         *
         * Restores dense RF bursts (up to TX_BURST_BUDGET_MS of write_fast calls per
         * loop iteration) while still yielding so ESPHome can service WiFi/API.
         * Cooldown applies between distinct queued payloads, not between individual
         * packet repeats within a payload. The radio is powered down for that
         * cooldown so the 3.3 V rail can recover, then given Tpd2stby after
         * power-up before the next burst.
         */
        void DirectolorRadio::send_code()
        {
            if (!this->radio_->is_chip_connected())
                return;

            uint32_t now = millis();

            // Acquire next payload if idle
            if (this->current_sending_payload_.send_attempts <= 0)
            {
                // Cooldown between distinct code payloads (not between packet repeats)
                if (this->last_payload_finish_ms_ != 0 &&
                    (now - this->last_payload_finish_ms_ < this->cooldown_))
                {
                    return;
                }

                if (this->continue_current_ && this->current_active_)
                {
                    if (!this->prepare_current_payload_())
                    {
                        this->current_active_ = false;
                        this->continue_current_ = false;
                        return;
                    }
                }
                else
                {
                    if (!this->queue_.dequeue(this->current_sending_payload_))
                        return;

                    this->current_active_ = true;
                    this->continue_current_ = false;
                    if (this->current_sending_payload_.logical && !this->prepare_current_payload_())
                    {
                        this->current_active_ = false;
                        return;
                    }
                }

                ESP_LOGV(TAG, "Processing - send_attempts: %d: %s",
                         this->current_sending_payload_.send_attempts,
                         format_hex_pretty(this->current_sending_payload_.payload, MAX_NRF_PAYLOAD_SIZE).c_str());

                this->radio_->power_up();
                this->radio_power_up_ms_ = now;
                this->radio_->stop_listening(); // put radio in TX mode
                this->radio_->set_address_width(3);
                this->radio_->open_writing_pipe(0x060406);

                if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE)
                {
                    this->dump_config();
                    this->radio_->dump_config();
                }
            }

            // Oscillator must settle after leaving power-down before CE goes high
            if (now - this->radio_power_up_ms_ < TX_POWER_UP_SETTLE_MS)
                return;

            // Dense burst: write as many packets as possible within the budget, then
            // yield so ESPHome can service WiFi/API. Matches pre-nonblocking density.
            const uint32_t burst_start = millis();
            while (this->current_sending_payload_.send_attempts > 0)
            {
                ESP_LOGV(TAG, "sending code (attempts left: %d)", this->current_sending_payload_.send_attempts);

                this->radio_->write_fast(this->current_sending_payload_.payload, this->radio_->get_payload_size(), true);
                this->current_sending_payload_.send_attempts--;

                if (this->current_sending_payload_.send_attempts == 0)
                {
                    this->radio_->tx_standby();
                    this->last_payload_finish_ms_ = millis();
                    ESP_LOGV(TAG, "send code complete");

                    if (this->current_sending_payload_.logical &&
                        this->current_sending_payload_.code_attempts_left > 0)
                    {
                        this->current_sending_payload_.code_attempts_left--;
                    }
                    if (this->current_sending_payload_.logical &&
                        this->current_sending_payload_.code_attempts_left > 0)
                    {
                        // Another distinct random/CRC of the same command after cooldown.
                        this->current_sending_payload_.payload_ready = false;
                        this->continue_current_ = true;
                        this->current_active_ = true;
                        this->radio_->power_down();
                        return;
                    }

                    this->continue_current_ = false;
                    this->current_active_ = false;
                    this->current_sending_payload_.logical = false;
                    if (this->queue_.isEmpty())
                    {
                        this->enterRemoteCaptureMode();
                    }
                    else
                    {
                        // Let the 3.3 V rail recover before the next distinct payload
                        this->radio_->power_down();
                    }
                    return;
                }

                // Flush the TX FIFO every 3 packets; yield when burst budget expires
                if (this->current_sending_payload_.send_attempts % 3 == 0)
                {
                    this->radio_->tx_standby();
                    if (millis() - burst_start >= TX_BURST_BUDGET_MS)
                        return;
                }
            }
        }

    }
}

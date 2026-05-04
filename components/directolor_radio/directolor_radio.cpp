#include "directolor_radio.h"

#include <esphome/core/log.h>
#include "esphome.h"
#include "esp_random.h"
#include <string>

namespace esphome
{
    namespace directolor_radio
    {
        static const char *TAG = "directolor_radio";
        static const uint8_t MATCHPATTERN[4] = {0xC0, 0X11, 0X00, 0X05}; // this is what we use to find out the codes for a new remote

        void DirectolorRadio::setup()
        {
            this->current_sending_payload_.send_attempts = 0;

            this->radio_->add_on_data_callback([this](const uint8_t *data, uint8_t len)
                                               { this->process_incoming_packet(data, len); });
            this->radio_->set_auto_ack(false);
            this->radio_->set_crc_length(nRF24L01::RF24_CRC_DISABLED);
        }

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

        void DirectolorRadio::process_incoming_packet(const uint8_t *payload, uint8_t bytes)
        {
            // 1. Sniffing / Learning Mode
            if (this->CaptureState_ == REMOTE_STATE_LEARNING)
            {
                ESP_LOGV(TAG, "Checking payload for match: %s", format_hex_pretty(payload, bytes).c_str());

                uint8_t foundPattern = 0;
                for (int i = 0; i < bytes; i++)
                {
                    if (payload[i] != MATCHPATTERN[foundPattern])
                    {
                        foundPattern = 0;
                    }
                    else
                    {
                        foundPattern++;
                    }

                    // Logic check: if pattern found, extract the address
                    if (foundPattern > 3 && i > 4)
                    {
                        this->sniffed_remote_code_[0] = payload[i - 5];
                        this->sniffed_remote_code_[1] = payload[i - 4];
                        this->sniffed_remote_code_[2] = payload[i + 4];
                        this->sniffed_remote_code_[3] = payload[i + 5];

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

            // Debounce: ignore repeated packets within 500ms
            uint32_t now = millis();
            if (now - this->lastMillis_ < 500)
            {
                return;
            }
            this->lastMillis_ = now;

            const char *command = "ERROR";

            // Standard Command Check (0x86)
            if (payload[4] == 0xFF && payload[5] == 0xFF &&
                payload[6] == this->sniffed_remote_code_[2] &&
                payload[7] == this->sniffed_remote_code_[3] && payload[8] == 0x86)
            {
                command = blind_action_to_string(static_cast<BlindAction>(payload[expected_bytes - 5]));
            }

            // Join/Remove Check (0x08)
            if (payload[4] == 0xFF && payload[5] == 0xFF &&
                payload[6] == this->sniffed_remote_code_[2] &&
                payload[7] == this->sniffed_remote_code_[3] && payload[8] == 0x08)
            {
                command = blind_action_to_string(static_cast<BlindAction>(payload[expected_bytes - 4]));
            }

            // Duplicate Check (0xC8)
            if (payload[0] == 0x12 && payload[1] == 0x80 && payload[2] == 0x0D &&
                payload[4] == 0xFF && payload[5] == 0xFF &&
                payload[16] == this->sniffed_remote_code_[2] &&
                payload[17] == this->sniffed_remote_code_[3] && payload[18] == 0xC8)
            {
                command = "Duplicate";
            }

            if (strcmp(command, "ERROR") == 0)
            {
                return;
            }

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
        }

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
                if (esp_log_get_default_level() >= ESP_LOG_VERBOSE)
                {
                    this->dump_config();
                }
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
            if (this->sniffed_remote_code_[0] || this->sniffed_remote_code_[1])
            {
                this->radio_->stop_listening();
                this->radio_->set_address_width(3);

                uint8_t capture_addr[3];
                capture_addr[2] = this->sniffed_remote_code_[0];
                capture_addr[1] = this->sniffed_remote_code_[1];
                capture_addr[0] = 0xC0;
                this->radio_->open_reading_pipe(1, capture_addr);

                this->radio_->start_listening(); // put radio in RX mode
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

        void DirectolorRadio::sendPayload(uint8_t *payload)
        {
            this->queue_.enqueue(payload, this->message_send_repeats_);
        }

        void DirectolorRadio::send_code()
        {
            uint32_t now = millis();
            if ((this->current_sending_payload_.send_attempts == this->message_send_repeats_) && (now - this->lastSendAttemptMillis_ < this->cooldown_))
                return;

            this->lastSendAttemptMillis_ = now;

            if (!this->radio_->is_chip_connected())
                return;
            if (this->current_sending_payload_.send_attempts == 0)
            {
                if (this->queue_.dequeue(this->current_sending_payload_))
                {
                    ESP_LOGV(TAG, "Processing - send_attempts: %d: %s",
                             this->current_sending_payload_.send_attempts,
                             format_hex_pretty(this->current_sending_payload_.payload, esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE).c_str());

                    this->radio_->power_up();
                    this->radio_->stop_listening(); // put radio in TX mode
                    this->radio_->set_address_width(3);
                    this->radio_->open_writing_pipe(0x060406);

                    if (esp_log_get_default_level() >= ESP_LOG_VERBOSE)
                        this->dump_config();
                }
            }
            else
            {
                ESP_LOGV(TAG, "sending code (attempts left: %d)", this->current_sending_payload_.send_attempts);

                unsigned long startMillis = millis();
                while (--this->current_sending_payload_.send_attempts > 0)
                {
                    this->radio_->write_fast(this->current_sending_payload_.payload, this->radio_->get_payload_size(), true); // we aren't waiting for an ACK, so we need to writeFast with multiCast set to true
                    if (this->current_sending_payload_.send_attempts % 3 == 0)
                    {
                        this->radio_->tx_standby();
                        if (millis() - startMillis > 25)
                            break;
                    }
                }
                this->radio_->tx_standby();
                if (this->current_sending_payload_.send_attempts == 0)
                {
                    ESP_LOGV(TAG, "send code complete");
                    if (!queue_.dequeue(this->current_sending_payload_))
                        this->enterRemoteCaptureMode(); // go back and power down
                }
            }
        }

    }
}
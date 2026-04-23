#include "directolor_cover.h"
#include <esphome/core/log.h>
#include "esphome.h"
#include "esp_random.h"
#include <string>

namespace esphome
{
    namespace directolor_cover
    {
        static const char *TAG = "directolor_cover";
        static const uint8_t MATCHPATTERN[4] = {0xC0, 0X11, 0X00, 0X05}; // this is what we use to find out the codes for a new remote
        static inline uint32_t last_millis = 0;

        void DirectolorCover::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Directolor Cover '%s'", this->name_.c_str());
            this->radio_->dump_config();
        }

        cover::CoverTraits DirectolorCover::get_traits()
        {
            auto traits = cover::CoverTraits();
            traits.set_supports_position(this->movement_duration_ms_ != 0);
            traits.set_supports_tilt(this->tilt_supported_);
            traits.set_supports_stop(true);
            traits.set_is_assumed_state(true);
            return traits;
        }

        void DirectolorCover::process_outstanding_send_attempts()
        {
            BlindAction curr_action = this->current_action_;

            if ((curr_action == directolor_join || curr_action == directolor_remove) && (outstanding_send_attempts_ % 2 == 0))
            {
                curr_action = directolor_duplicate;
            }

            this->outstanding_send_attempts_--;
            uint8_t payload[this->radio_->getPayloadSize()];

            int length = this->get_radio_command(payload, curr_action);

            uint16_t crc = crc16be((uint8_t *)payload, length, 0xFFFF, 0x755b, false, false); // took some time to figure this out.  big thanks to CRC RevEng by Gregory Cook!!!!  CRC is calculated over the whole payload, including radio id at start.
            ESP_LOGV(TAG, "payload: %s  crc: 0x%04X", format_hex_pretty(payload, length).c_str(), crc);
            payload[length++] = crc >> 8;
            payload[length] = crc & 0xFF;

            for (int i = this->radio_->getPayloadSize(); i > 0; i--) // pad with leading 0x55 to train the shade receivers
            {
                if (i - (this->radio_->getPayloadSize() - length) >= 0)
                    payload[i - 1] = payload[i - (this->radio_->getPayloadSize() - length)];
                else
                    payload[i - 1] = 0x55;
            }

            this->sendPayload(payload);
        }

        uint32_t last_heartbeat = 0;

        void DirectolorCover::loop()
        {
            this->checkRadioPayload();
            if (this->outstanding_send_attempts_ > 0)
            {
                process_outstanding_send_attempts();
            }

            this->send_code();
        }

        void DirectolorCover::setup()
        {
            ESP_LOGCONFIG(TAG, "Setting up Directolor Cover '%s'", this->get_name().c_str());
            this->current_sending_payload_.send_attempts = 0;
            this->command_random_ = esp_random() % 256;
            this->radioValid = false;
        }

        void DirectolorCover::control(const cover::CoverCall &call)
        {
            // Kill any existing timer immediately when a new command arrives
            this->cancel_timeout("delayed_stop");

            if (call.get_position().has_value())
            {
                float pos = *call.get_position();

                if (pos == cover::COVER_OPEN)
                {
                    this->issue_shade_command(directolor_open);
                }
                else if (pos == cover::COVER_CLOSED)
                {
                    this->issue_shade_command(directolor_close);
                }
                else if (pos == this->position)
                {
                    ESP_LOGI(TAG, "Shade '%s' already at requested position.", this->get_name().c_str());
                }
                else
                {
                    if (this->position > pos)
                        this->issue_shade_command(directolor_close);
                    else
                        this->issue_shade_command(directolor_open);

                    uint32_t delay = static_cast<uint32_t>(this->movement_duration_ms_ * std::abs(this->position - pos));

                    this->set_timeout("delayed_stop", delay, [this, pos, delay]()
                                      { 
                                        this->issue_shade_command(directolor_stop); 
                                        ESP_LOGD(TAG, "Scheduled stop executed for %s", this->get_name().c_str()); });
                    ESP_LOGD(TAG, "%s scheduled for stop after delay. Position: %.2f, Target: %.2f, Delay: %u ms", this->get_name().c_str(), this->position, pos, delay);
                }

                this->position = pos;
                if (this->tilt_supported_)
                    this->tilt = 0;
                this->publish_state();
            }

            if (call.get_stop())
            {
                this->issue_shade_command(directolor_stop);
                this->publish_state();
            }

            if (call.get_tilt().has_value())
            {
                float tilt_val = *call.get_tilt();

                if (tilt_val == 0.0f)
                {
                    this->issue_shade_command(directolor_tiltClose);
                }
                else if (tilt_val == 1.0f)
                {
                    this->issue_shade_command(directolor_tiltOpen);
                }
                else
                {
                    if (this->tilt == tilt_val)
                    {
                        ESP_LOGD(TAG, "Current tilt matches requested tilt.");
                        return;
                    }

                    if (this->tilt > tilt_val)
                        this->issue_shade_command(directolor_tiltClose);
                    else
                        this->issue_shade_command(directolor_tiltOpen);

                    uint32_t delay = static_cast<uint32_t>(DEFAULT_TILT_DURATION_MS * std::abs(this->tilt - tilt_val));

                    this->set_timeout("delayed_stop", delay, [this, tilt_val, delay]()
                                      { 
                                        this->issue_shade_command(directolor_stop); 
                                        ESP_LOGD(TAG, "Tilt scheduled stop executed for %s", this->get_name().c_str()); });
                    ESP_LOGD(TAG, "%s tilt scheduled for stop after delay. Current: %.2f, Target: %.2f, Delay: %u ms", this->get_name().c_str(), this->tilt, tilt_val, delay);
                }
                this->tilt = tilt_val;
                this->publish_state();
            }
        }

        const char *blind_action_to_string(BlindAction action)
        {
            switch (action)
            {
            case directolor_open:
                return "open";
            case directolor_close:
                return "close";
            case directolor_stop:
                return "stop";
            case directolor_join:
                return "join";
            case directolor_remove:
                return "remove";
            case directolor_duplicate:
                return "duplicate";
            default:
                return "unknown";
            }
        }

        void DirectolorCover::issue_shade_command(BlindAction blind_action)
        {
            ESP_LOGI(TAG, "Issuing shade command for '%s': action=%s, copies=%d",
                     this->get_name().c_str(), blind_action_to_string(blind_action), this->message_send_repeats_);
            this->current_action_ = blind_action;
            this->outstanding_send_attempts_ = this->code_attempts_;
        }

        static constexpr uint8_t duplicatePrototype[] = {0XFF, 0XFF, 0xC0, 0X12, 0X80, 0X0D, 0x67, 0XFF, 0XFF, 0XC4, 0X05, 0XB1, 0XEC, 0X1D, 0XE3, 0X98, 0x8B, 0X2D, 0XDE, 0X00, 0XEF, 0XC8}; // 6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23

        int DirectolorCover::get_duplicate_radio_command(uint8_t *payload, BlindAction blind_action)
        {
            const uint8_t offset = 0;
            int payloadOffset = 0;
            // int uniqueBytesOffset = i * DUPLICATE_CODE_UNIQUE_BYTES;
            for (int j = 0; j < sizeof(duplicatePrototype); j++)
            {
                switch (j) // 6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23
                {
                case 6 + offset:
                    payload[payloadOffset + j] = command_random_++;
                    break;
                case 9 + offset:
                    payload[payloadOffset + j] = 0x06;
                    break;
                case 10 + offset:
                    payload[payloadOffset + j] = 0x03;
                    break;
                case 11 + offset:
                    payload[payloadOffset + j] = 0x20;
                    break;
                case 12 + offset:
                    payload[payloadOffset + j] = 0x05;
                    break;
                case 13 + offset:
                    payload[payloadOffset + j] = 0x12;
                    break;
                case 14 + offset:
                    payload[payloadOffset + j] = 0x03;
                    break;
                case 15 + offset:
                    payload[payloadOffset + j] = 0xAC;
                    break;
                case 16 + offset:
                    payload[payloadOffset + j] = 0x56;
                    break;
                case 17 + offset:
                    payload[payloadOffset + j] = this->radio_code_[1];
                    break;
                case 18 + offset:
                    payload[payloadOffset + j] = this->radio_code_[0];
                    break;
                case 19 + offset:
                    payload[payloadOffset + j] = this->radio_code_[2];
                    break;
                case 20 + offset:
                    payload[payloadOffset + j] = this->radio_code_[3];
                    break;
                default:
                    payload[payloadOffset + j] = duplicatePrototype[j];
                    break;
                }
            }
            return sizeof(duplicatePrototype);
        }

        static constexpr uint8_t groupPrototype[] = {0X11, 0X11, 0xC0, 0X0A, 0X40, 0X05, 0X18, 0XFF, 0XFF, 0X8A, 0X91, 0X08, 0X03, 0X01}; // 0, 1, 6, 9, 10, 12, 13, 14, 15

        int DirectolorCover::get_group_radio_command(uint8_t *payload, BlindAction blind_action)
        {
            const uint8_t offset = 0;
            int payloadOffset = 0;
            for (int j = 0; j < sizeof(groupPrototype); j++)
            {
                switch (j) // 0, 1, 6, 9, 10, 12, 13, 14, 15
                {
                case 0 + offset:
                    payload[payloadOffset + j] = this->radio_code_[0];
                    break;
                case 1 + offset:
                    payload[payloadOffset + j] = this->radio_code_[1];
                    break;
                case 6 + offset:
                    payload[payloadOffset + j] = command_random_++;
                    break;
                case 9 + offset:
                    payload[payloadOffset + j] = this->radio_code_[2];
                    break;
                case 10 + offset:
                    payload[payloadOffset + j] = this->radio_code_[3];
                    break;
                case 12 + offset:
                    payload[payloadOffset + j] = this->channel_;
                    break;
                case 13 + offset:
                    payload[payloadOffset + j] = blind_action;
                    break;
                default:
                    payload[payloadOffset + j] = groupPrototype[j];
                    break;
                }
            }
            return sizeof(groupPrototype);
        }

        static constexpr uint8_t setFavPrototype[] = {0X11, 0X11, 0xC0, 0X0F, 0X00, 0X05, 0XD1, 0XFF, 0XFF, 0XB0, 0X51, 0X86, 0X04, 0XB8, 0XB0, 0X51, 0X63, 0X49, 0X00}; // 0, 1, 6, 9, 10, 12, 13, 14, 15

        int DirectolorCover::get_set_fav_radio_command(uint8_t *payload, BlindAction blind_action)
        {
            const uint8_t offset = 0;
            int payloadOffset = 0;
            int j = 0;
            while (j + payloadOffset < this->radio_->getPayloadSize())
            {
                switch (j) // 0, 1, 6, 9, 10, 12, 13, 14, 15
                {
                case 0 + offset:
                    payload[payloadOffset + j] = this->radio_code_[0];
                    break;
                case 1 + offset:
                    payload[payloadOffset + j] = this->radio_code_[1];
                    break;
                case 6 + offset:
                    payload[payloadOffset + j] = this->command_random_++;
                    break;
                case 9 + offset:
                    payload[payloadOffset + j] = this->radio_code_[2];
                    break;
                case 10 + offset:
                    payload[payloadOffset + j] = this->radio_code_[3];
                    break;
                case 13 + offset:
                    payload[payloadOffset + j] = this->command_random_ + 122;
                    break;
                case 14 + offset:
                    payload[payloadOffset + j] = this->radio_code_[2];
                    break;
                case 15 + offset:
                    payload[payloadOffset + j] = this->radio_code_[3];
                    break;
                default:
                    payload[payloadOffset + j] = setFavPrototype[j];
                    break;
                }
                j++;
            }

            return sizeof(setFavPrototype) + payloadOffset;
        }

        static constexpr uint8_t commandPrototype[] = {0X11, 0X11, 0xC0, 0X10, 0X00, 0X05, 0XBC, 0XFF, 0XFF, 0X8A, 0X91, 0X86, 0X06, 0X99, 0X01, 0X00, 0X8A, 0X91, 0X52, 0X53, 0X00};

        int DirectolorCover::get_radio_command(uint8_t *payload, BlindAction blind_action)
        {
            switch (blind_action)
            {
            case directolor_join:
            case directolor_remove:
                return this->get_group_radio_command(payload, blind_action);
            case directolor_duplicate:
                return this->get_duplicate_radio_command(payload, blind_action);
            case directolor_setFav:
                return this->get_set_fav_radio_command(payload, blind_action);
            }
            const uint8_t offset = 0;
            int payloadOffset = 0;
            int j = 0;
            while (j + payloadOffset < this->radio_->getPayloadSize())
            {
                switch (j) // 0, 1, 6, 9, 10, 12, 13, 14, 15
                {
                case 0 + offset:
                    payload[payloadOffset + j] = this->radio_code_[0];
                    break;
                case 1 + offset:
                    payload[payloadOffset + j] = this->radio_code_[1];
                    break;
                case 6 + offset:
                    payload[payloadOffset + j] = this->command_random_++;
                    break;
                case 9 + offset:
                    payload[payloadOffset + j] = this->radio_code_[2];
                    break;
                case 10 + offset:
                    payload[payloadOffset + j] = this->radio_code_[3];
                    break;
                case 13 + offset:
                    payload[payloadOffset + j] = this->command_random_ + 122;
                    break;
                case 14 + offset:
                    payload[j + payloadOffset++] = this->channel_;
                    payload[3]++;
                    payloadOffset--;
                    break;
                case 16 + offset:
                    payload[payloadOffset + j] = this->radio_code_[2];
                    break;
                case 17 + offset:
                    payload[payloadOffset + j] = this->radio_code_[3];
                    break;
                case 19 + offset:
                    payload[payloadOffset + j] = blind_action;
                    break;
                default:
                    payload[payloadOffset + j] = commandPrototype[j];
                    break;
                }
                j++;
            }

            return sizeof(commandPrototype) + payloadOffset;
        }

        bool DirectolorCover::radioStarted()
        {
            //     if (this->radio_->failureDetected)
            //     {
            //         this->radioValid = false;
            //         this->radio_->failureDetected = false;
            //     }

            if (!this->radioValid)
            {
                if (millis() - this->lastStartAttempt < currentCooldown)
                    return false; // don't try to restart too often

                if (currentCooldown < 60000)
                    currentCooldown *= 2; // back off up to 1 minute

                lastStartAttempt = millis();

                ESP_LOGI(TAG, "attempting to start radio");

                this->radioValid = this->radio_->begin();

                if (this->radioValid)
                {
                    this->radio_->stop_listening();                          // make sure we're not still in listening mode from a previous session
                    this->radio_->setAutoAck(false);                         // auto-ack has to be off or everything breaks because I haven't been able to RE the protocol CRC / validation
                    this->radio_->setCRCLength(nRF24L01::RF24_CRC_DISABLED); // disable CRC

                    ESP_LOGI(TAG, "radio started");
                    this->enterRemoteSearchMode();
                }
                else
                {
                    ESP_LOGE(TAG, "Failure starting radio");
                }
            }

            return this->radioValid;
        }

        bool DirectolorCover::enterRemoteSearchMode()
        {
            if (this->radioStarted())
            {
                ESP_LOGI(TAG, "attempting to start listening");
                this->radio_->stop_listening();
                this->radio_->set_address_width(5);
                this->radio_->open_reading_pipe(1, 0x5555555555); // always uses pipe 0
                ESP_LOGI(TAG, "searching for remote");
                this->radio_->start_listening(); // put radio in RX mode
                this->learningRemote = true;
                ESP_LOGI(TAG, "started listening - press a button on your remote");
                if (esp_log_get_default_level() >= ESP_LOG_VERBOSE)
                {
                    this->dump_config();
                }
                return true;
            }
            return false;
        }

        void DirectolorCover::enterRemoteCaptureMode()
        {
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
            else if (learningRemote)
            {
                enterRemoteSearchMode();
            }
            else
            {
                this->radio_->powerDown();
            }
        }

        void DirectolorCover::sendPayload(uint8_t *payload)
        {
            this->queue_.enqueue(payload, this->message_send_repeats_);
        }

        void DirectolorCover::send_code()
        {
            if (!this->radioStarted())
                return;
            if (this->current_sending_payload_.send_attempts == 0)
            {
                if (queue_.dequeue(this->current_sending_payload_))
                {
                    ESP_LOGV(TAG, "Processing - send_attempts: %d: %s",
                             this->current_sending_payload_.send_attempts,
                             format_hex_pretty(this->current_sending_payload_.payload, 32).c_str());

                    this->radio_->powerUp();
                    this->radio_->stop_listening(); // put radio in TX mode
                    this->radio_->set_address_width(3);
                    this->radio_->open_writing_pipe(0x060406);
                    //                    this->radio_->set_payload_size(MAX_PAYLOAD_SIZE);  //TOdo; remove
                }
            }
            else
            {
                ESP_LOGV(TAG, "sending code (attempts left: %d)", this->current_sending_payload_.send_attempts);

                unsigned long startMillis = millis();
                while (--this->current_sending_payload_.send_attempts > 0)
                {
                    this->radio_->writeFast(this->current_sending_payload_.payload, this->radio_->getPayloadSize(), true); // we aren't waiting for an ACK, so we need to writeFast with multiCast set to true
                    if (this->current_sending_payload_.send_attempts % 3 == 0)
                    {
                        this->radio_->txStandBy();
                        if (millis() - startMillis > 25)
                            break;
                    }
                }
                this->radio_->txStandBy();
                if (this->current_sending_payload_.send_attempts == 0)
                {
                    ESP_LOGV(TAG, "send code complete");
                    if (!queue_.dequeue(this->current_sending_payload_))
                        this->enterRemoteCaptureMode(); // go back and power down
                }
            }
        }

        void DirectolorCover::checkRadioPayload() // could modify this to allow capture if commands were sent using multiple channels
        {
            if (this->radioStarted())
            {
                uint8_t pipe;
                if (this->radio_->available(&pipe))
                {
                    uint8_t bytes = this->radio_->getPayloadSize(); // get the size of the payload
                    char payload[bytes];
                    this->radio_->read(payload, bytes); // fetch payload from FIFO

                    if (this->learningRemote)
                    {
                        ESP_LOGV(TAG, "checking payload for match: %s", format_hex_pretty(reinterpret_cast<const uint8_t *>(payload), bytes).c_str());

                        uint8_t foundPattern = 0;

                        for (int i = 0; i < bytes; i++)
                        {
                            if (payload[i] != MATCHPATTERN[foundPattern])
                                foundPattern = 0;
                            else
                                foundPattern++;

                            if (foundPattern > 3 && i > 4)
                            {
                                this->sniffed_remote_code_[0] = payload[i - 5];
                                this->sniffed_remote_code_[1] = payload[i - 4];
                                this->sniffed_remote_code_[2] = payload[i + 4];
                                this->sniffed_remote_code_[3] = payload[i + 5];
                                this->learningRemote = false;
                                ESP_LOGI(TAG, "Found Remote with address: [0x%02X, 0x%02X, 0x%02X, 0x%02X]",
                                         this->sniffed_remote_code_[0], this->sniffed_remote_code_[1], this->sniffed_remote_code_[2], this->sniffed_remote_code_[3]);
                                this->enterRemoteCaptureMode();
                            }
                        }
                    }
                    else
                    {
                        bytes = payload[0] + 4;
                        if (bytes > 32)
                            return; // invalid payload - discarding

                        if (esp_log_get_default_level() <= ESP_LOG_DEBUG)
                        {
                            bool skip = (millis() - last_millis < 25);
                            last_millis = millis();
                            if (skip)
                                return;
                        }

                        const char* command = "ERROR";

                        if (payload[4] == 0xFF && payload[5] == 0xFF && payload[6] == this->sniffed_remote_code_[2] && payload[7] == this->sniffed_remote_code_[3] && payload[8] == 0x86)
                            switch (payload[bytes - 5])
                            {
                            case directolor_open:
                                command = "Open";
                                break;
                            case directolor_close:
                                command = "Close";
                                break;
                            case directolor_tiltOpen:
                                command = "Tilt Open";
                                break;
                            case directolor_tiltClose:
                                command = "Tilt Close";
                                break;
                            case directolor_stop:
                                command = "Stop";
                                break;
                            case directolor_toFav:
                                command = "to Fav";
                                break;
                            case directolor_setFav:
                                command = "Store Favorite";
                            }

                        if (payload[4] == 0xFF && payload[5] == 0xFF && payload[6] == this->sniffed_remote_code_[2] && payload[7] == this->sniffed_remote_code_[3] && payload[8] == 0x08)
                        {
                            switch (payload[bytes - 4])
                            {
                            case directolor_join:
                                command = "Join";
                                break;
                            case directolor_remove:
                                command = "Remove";
                                break;
                            }
                        }

                        if (payload[0] == 0x12 && payload[1] == 0x80 && payload[2] == 0x0D && payload[4] == 0xFF && payload[5] == 0xFF && payload[16] == this->sniffed_remote_code_[2] && payload[17] == this->sniffed_remote_code_[3] && payload[18] == 0xC8)
                            command = "Duplicate";

                        if (command == "ERROR")
                            return;

                        ESP_LOGV(TAG, "bytes: %d pipe: %d: %s", bytes, pipe, format_hex_pretty(reinterpret_cast<const uint8_t *>(payload), bytes).c_str());
                        char buffer[12];
                        ESP_LOGI(TAG, "Received %s from: %s", command, format_hex_pretty_to(buffer, this->sniffed_remote_code_.data(), this->sniffed_remote_code_.size()));
                    }
                }
            }
        }

    } // namespace directolor_cover
} // namespace esphome
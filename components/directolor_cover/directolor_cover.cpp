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
        static inline uint32_t last_millis = 0;

        void DirectolorCover::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Directolor Cover '%s'", this->name_.c_str());
            ESP_LOGCONFIG(TAG, "  Radio Code: %s", format_hex_pretty(radio_code_.data(), radio_code_.size()).c_str());
            ESP_LOGCONFIG(TAG, "  Movement Duration: %.2f seconds", this->movement_duration_ms_ / 1000.0f);
            ESP_LOGCONFIG(TAG, "  Tilt Supported: %s", this->tilt_supported_ ? "Yes" : "No");
            ESP_LOGCONFIG(TAG, "  Channel: %d", this->channel_);
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

        void DirectolorCover::setup()
        {
            ESP_LOGCONFIG(TAG, "Setting up Directolor Cover '%s'", this->get_name().c_str());
            this->command_random_ = esp_random() % 256;
        }

        void DirectolorCover::loop()
        {
            while (this->outstanding_retry_count_ > 0)
            {
                create_and_send_payload(this->current_blind_action_);

                if (this->current_blind_action_ == directolor_join || this->current_blind_action_ == directolor_remove)
                {
                    create_and_send_payload(directolor_duplicate);
                    this->outstanding_retry_count_ = 0; // don't retry join/remove commands since they are only needed once and can cause issues if repeated
                    return;
                }

                if (this->outstanding_retry_count_ == this->hub_->get_code_attempts())
                    return;

                this->outstanding_retry_count_--;
            }
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

                    uint32_t delay = static_cast<uint32_t>(esphome::directolor_radio::DEFAULT_TILT_DURATION_MS * std::abs(this->tilt - tilt_val));

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

        void DirectolorCover::create_and_send_payload(BlindAction blind_action)
        {
            uint8_t payload[esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE];

            int length = this->get_radio_command(payload, blind_action);

            uint16_t crc = crc16be((uint8_t *)payload, length, 0xFFFF, 0x755b, false, false); // took some time to figure this out.  big thanks to CRC RevEng by Gregory Cook!!!!  CRC is calculated over the whole payload, including radio id at start.
            ESP_LOGV(TAG, "payload: %s  crc: 0x%04X", format_hex_pretty(payload, length).c_str(), crc);
            payload[length++] = crc >> 8;
            payload[length] = crc & 0xFF;

            for (int i = esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE; i > 0; i--) // pad with leading 0x55 to train the shade receivers
            {
                if (i - (esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE - length) >= 0)
                    payload[i - 1] = payload[i - (esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE - length)];
                else
                    payload[i - 1] = 0x55;
            }

            this->hub_->sendPayload(payload);
        }

        void DirectolorCover::issue_shade_command(BlindAction blind_action)
        {
            ESP_LOGI(TAG, "Issuing shade command for '%s': action=%s", this->get_name().c_str(), blind_action_to_string(blind_action));
            this->current_blind_action_ = blind_action;
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
            while (j + payloadOffset < esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE)
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
            while (j + payloadOffset < esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE)
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

    } // namespace directolor_cover
} // namespace esphome
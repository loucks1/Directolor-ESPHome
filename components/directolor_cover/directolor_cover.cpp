#include "directolor_cover.h"
#include "esphome/components/directolor_radio/directolor_radio.h"
#include <esphome/core/log.h>
#include "esphome.h"
#include "esp_random.h"
#include <cstring>
#include <cmath>
#include <string>

namespace esphome
{
    namespace directolor_cover
    {
        static const char *TAG = "directolor_cover";

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

                    this->set_timeout("delayed_stop", delay, [this, pos]()
                                      {
                                        this->issue_shade_command(directolor_stop);
                                        ESP_LOGD(TAG, "Scheduled stop executed for %s", this->get_name().c_str()); });
                    ESP_LOGD(TAG, "%s scheduled for stop after delay. Position: %.2f, Target: %.2f, Delay: %lu ms", this->get_name().c_str(), this->position, pos, (unsigned long)delay);
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

                    this->set_timeout("delayed_stop", delay, [this]()
                                      {
                                        this->issue_shade_command(directolor_stop);
                                        ESP_LOGD(TAG, "Tilt scheduled stop executed for %s", this->get_name().c_str()); });
                    ESP_LOGD(TAG, "%s tilt scheduled for stop after delay. Current: %.2f, Target: %.2f, Delay: %lu ms", this->get_name().c_str(), this->tilt, tilt_val, (unsigned long)delay);
                }
                this->tilt = tilt_val;
                this->publish_state();
            }
        }

        void DirectolorCover::create_and_send_payload(BlindAction blind_action)
        {
            uint8_t payload[esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE];
            int length = this->get_radio_command(payload, blind_action);

            if (length <= 0 || length > static_cast<int>(esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE))
            {
                ESP_LOGE(TAG, "payload length %d exceeds max %d", length, static_cast<int>(esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE));
                return;
            }

            // CRC is calculated over the whole payload, including radio id at start.
            // Big thanks to CRC RevEng by Gregory Cook.
            uint16_t crc = crc16be((uint8_t *)payload, length, 0xFFFF, 0x755b, false, false);
            ESP_LOGV(TAG, "payload: %s  crc: 0x%04X", format_hex_pretty(payload, length).c_str(), crc);

            payload[length++] = crc >> 8;
            payload[length++] = crc & 0xFF;

            // Right-align the real bytes within the buffer, padding the leading bytes
            // with 0x55 to train the shade receivers.
            const int pad_len = static_cast<int>(esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE) - length;
            if (pad_len > 0)
            {
                std::memmove(payload + pad_len, payload, length);
                std::memset(payload, 0x55, pad_len);
            }

            if (!this->hub_->sendPayload(payload))
            {
                ESP_LOGW(TAG, "Failed to enqueue command for '%s' (queue full)", this->get_name().c_str());
            }
        }

        void DirectolorCover::issue_shade_command(BlindAction blind_action)
        {
            ESP_LOGI(TAG, "Issuing shade command for '%s': action=%s", this->get_name().c_str(), this->hub_->blind_action_to_string(blind_action));

            // Join/remove are one-shot and always followed by a duplicate pairing frame.
            // Repeating them can re-trigger programming on the blind.
            if (blind_action == directolor_join || blind_action == directolor_remove)
            {
                create_and_send_payload(blind_action);
                create_and_send_payload(directolor_duplicate);
                return;
            }

            // Enqueue each code attempt immediately. Each attempt gets a unique
            // random/CRC so the shade treats them as distinct retransmissions.
            const uint8_t attempts = this->hub_->get_code_attempts();
            for (uint8_t i = 0; i < attempts; i++)
            {
                create_and_send_payload(blind_action);
            }
        }

        static constexpr uint8_t duplicatePrototype[] = {0XFF, 0XFF, 0xC0, 0X12, 0X80, 0X0D, 0x67, 0XFF, 0XFF, 0XC4, 0X05, 0XB1, 0XEC, 0X1D, 0XE3, 0X98, 0x8B, 0X2D, 0XDE, 0X00, 0XEF, 0XC8}; // 6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23

        int DirectolorCover::get_duplicate_radio_command(uint8_t *payload, BlindAction blind_action)
        {
            (void)blind_action;
            for (size_t j = 0; j < sizeof(duplicatePrototype); j++)
            {
                switch (j)
                {
                case 6:
                    payload[j] = command_random_++;
                    break;
                case 9:
                    payload[j] = 0x06;
                    break;
                case 10:
                    payload[j] = 0x03;
                    break;
                case 11:
                    payload[j] = 0x20;
                    break;
                case 12:
                    payload[j] = 0x05;
                    break;
                case 13:
                    payload[j] = 0x12;
                    break;
                case 14:
                    payload[j] = 0x03;
                    break;
                case 15:
                    payload[j] = 0xAC;
                    break;
                case 16:
                    payload[j] = 0x56;
                    break;
                case 17:
                    payload[j] = this->radio_code_[1];
                    break;
                case 18:
                    payload[j] = this->radio_code_[0];
                    break;
                case 19:
                    payload[j] = this->radio_code_[2];
                    break;
                case 20:
                    payload[j] = this->radio_code_[3];
                    break;
                default:
                    payload[j] = duplicatePrototype[j];
                    break;
                }
            }
            return static_cast<int>(sizeof(duplicatePrototype));
        }

        static constexpr uint8_t groupPrototype[] = {0X11, 0X11, 0xC0, 0X0A, 0X40, 0X05, 0X18, 0XFF, 0XFF, 0X8A, 0X91, 0X08, 0X03, 0X01}; // 0, 1, 6, 9, 10, 12, 13, 14, 15

        int DirectolorCover::get_group_radio_command(uint8_t *payload, BlindAction blind_action)
        {
            for (size_t j = 0; j < sizeof(groupPrototype); j++)
            {
                switch (j)
                {
                case 0:
                    payload[j] = this->radio_code_[0];
                    break;
                case 1:
                    payload[j] = this->radio_code_[1];
                    break;
                case 6:
                    payload[j] = command_random_++;
                    break;
                case 9:
                    payload[j] = this->radio_code_[2];
                    break;
                case 10:
                    payload[j] = this->radio_code_[3];
                    break;
                case 12:
                    payload[j] = this->channel_;
                    break;
                case 13:
                    payload[j] = blind_action;
                    break;
                default:
                    payload[j] = groupPrototype[j];
                    break;
                }
            }
            return static_cast<int>(sizeof(groupPrototype));
        }

        static constexpr uint8_t setFavPrototype[] = {0X11, 0X11, 0xC0, 0X0F, 0X00, 0X05, 0XD1, 0XFF, 0XFF, 0XB0, 0X51, 0X86, 0X04, 0XB8, 0XB0, 0X51, 0X63, 0X49, 0X00}; // 0, 1, 6, 9, 10, 12, 13, 14, 15

        int DirectolorCover::get_set_fav_radio_command(uint8_t *payload, BlindAction blind_action)
        {
            (void)blind_action;
            for (size_t j = 0; j < sizeof(setFavPrototype); j++)
            {
                switch (j)
                {
                case 0:
                    payload[j] = this->radio_code_[0];
                    break;
                case 1:
                    payload[j] = this->radio_code_[1];
                    break;
                case 6:
                    payload[j] = this->command_random_++;
                    break;
                case 9:
                    payload[j] = this->radio_code_[2];
                    break;
                case 10:
                    payload[j] = this->radio_code_[3];
                    break;
                case 13:
                    payload[j] = this->command_random_ + (esp_random() % 256);
                    break;
                case 14:
                    payload[j] = this->radio_code_[2];
                    break;
                case 15:
                    payload[j] = this->radio_code_[3];
                    break;
                default:
                    payload[j] = setFavPrototype[j];
                    break;
                }
            }
            return static_cast<int>(sizeof(setFavPrototype));
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
            default:
                break;
            }

            for (size_t j = 0; j < sizeof(commandPrototype); j++)
            {
                switch (j)
                {
                case 0:
                    payload[j] = this->radio_code_[0];
                    break;
                case 1:
                    payload[j] = this->radio_code_[1];
                    break;
                case 6:
                    payload[j] = this->command_random_++;
                    break;
                case 9:
                    payload[j] = this->radio_code_[2];
                    break;
                case 10:
                    payload[j] = this->radio_code_[3];
                    break;
                case 13:
                    payload[j] = this->command_random_ + (esp_random() % 256);
                    break;
                case 14:
                    // Channel overrides the prototype placeholder; length field accounts for it.
                    payload[j] = this->channel_;
                    payload[3]++;
                    break;
                case 16:
                    payload[j] = this->radio_code_[2];
                    break;
                case 17:
                    payload[j] = this->radio_code_[3];
                    break;
                case 19:
                    payload[j] = blind_action;
                    break;
                default:
                    payload[j] = commandPrototype[j];
                    break;
                }
            }

            return static_cast<int>(sizeof(commandPrototype));
        }

    } // namespace directolor_cover
} // namespace esphome

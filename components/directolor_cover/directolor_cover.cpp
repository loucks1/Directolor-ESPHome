#include "directolor_cover.h"
#include <esphome/core/log.h>
#include "esphome.h"
#include <string>

#define MS_FOR_FULL_TILT_MOVEMENT 5000

namespace esphome
{
    namespace directolor_cover
    {
        static const char *TAG = "directolor_cover";

        void DirectolorCover::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Directolor Cover '%s'", this->name_.c_str());
            // ESP_LOGCONFIG("directolor.cover", "  Radio Code: 0x%02X, 0x%02X", this->radio_id_, this->command_);
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

        void DirectolorCover::loop()
        {
            if (this->outstanding_send_attempts_ > 0)
            {
                this->outstanding_send_attempts_--;
                byte payload[MAX_PAYLOAD_SIZE];

                int length = this->get_radio_command(payload, this->current_action_);

                uint16_t crc = calcCRC16((uint8_t *)payload, length, 0x755b, 0xFFFF, 0, false, false); // took some time to figure this out.  big thanks to CRC RevEng by Gregory Cook!!!!  CRC is calculated over the whole payload, including radio id at start.
                payload[length++] = crc >> 8;
                payload[length] = crc & 0xFF;

                for (int i = MAX_PAYLOAD_SIZE; i > 0; i--) // pad with leading 0x55 to train the shade receivers
                {
                    if (i - (MAX_PAYLOAD_SIZE - length) >= 0)
                        payload[i - 1] = payload[i - (MAX_PAYLOAD_SIZE - length)];
                    else
                        payload[i - 1] = 0x55;
                }

                this->base_->sendPayload(payload);
            }

            if (this->ms_duration_for_delayed_stop_ != 0 && (millis() - this->start_of_timed_movement_ >= this->ms_duration_for_delayed_stop_))
            {
                this->issue_shade_command(directolor_stop, DIRECTOLOR_CODE_ATTEMPTS);
                this->ms_duration_for_delayed_stop_ = 0;
            }
        }

        void DirectolorCover::setup()
        {
            ESP_LOGCONFIG(TAG, "Setting up Directolor Cover '%s'", this->get_name().c_str());
            this->command_random_ = random(256);

            // Initialize and register the join switch
            if (this->program_function_support_)
            {
                this->duplicate_button_ = new ActionButton(this, "Duplicate");
                App.register_button(this->duplicate_button_);

                this->join_button_ = new ActionButton(this, "Join");
                App.register_button(this->join_button_);

                this->remove_button_ = new ActionButton(this, "Remove");
                App.register_button(this->remove_button_);
            }

            if (this->favorite_support_)
            {
                this->set_fav_button_ = new ActionButton(this, "Set Favorite");
                App.register_button(this->set_fav_button_);

                this->to_fav_button_ = new ActionButton(this, "To Favorite");
                App.register_button(this->to_fav_button_);
            }
        }

        void DirectolorCover::ActionButton::press_action()
        {
            this->parent_->on_action_button_press(this->id);
        }

        void DirectolorCover::on_action_button_press(std::string &id)
        {
            ESP_LOGD(TAG, "Action button pressed '%s' - %s", this->get_name().c_str(), id.c_str());
        }

        void DirectolorCover::control(const cover::CoverCall &call)
        {
            this->ms_duration_for_delayed_stop_ = 0;
            if (call.get_position().has_value())
            {
                float pos = *call.get_position();
                if (pos == cover::COVER_OPEN)
                {
                    this->issue_shade_command(directolor_open, DIRECTOLOR_CODE_ATTEMPTS); // Open command
                }
                else if (pos == cover::COVER_CLOSED)
                {
                    this->issue_shade_command(directolor_close, DIRECTOLOR_CODE_ATTEMPTS); // Close command
                }
                else if (pos == this->position)
                {
                    ESP_LOGI(TAG, "Shade '%s' already at requested position.", this->get_name().c_str());
                }
                else
                {
                    if (this->position > pos)
                        this->issue_shade_command(directolor_close, DIRECTOLOR_CODE_ATTEMPTS); // Close command
                    else
                        this->issue_shade_command(directolor_open, DIRECTOLOR_CODE_ATTEMPTS); // Open command
                    ESP_LOGD(TAG, "current position %.2f requested position %.2f total seconds for movement %d", this->position, pos, this->movement_duration_ms_);
                    this->ms_duration_for_delayed_stop_ = this->movement_duration_ms_ * abs(this->position - pos);
                    ESP_LOGD(TAG, "desiredDelay: %d", ms_duration_for_delayed_stop_);
                    this->start_of_timed_movement_ = millis();
                }

                this->position = pos;
                if (this->tilt_supported_)
                    this->tilt = 0;
                this->publish_state(true);
            }

            if (call.get_stop())
            {
                this->issue_shade_command(directolor_stop, DIRECTOLOR_CODE_ATTEMPTS);
            }

            if (call.get_tilt())
            {
                float tilt = *call.get_tilt();
                if (tilt == 0)
                {
                    this->issue_shade_command(directolor_tiltClose, DIRECTOLOR_CODE_ATTEMPTS);
                }
                else if (tilt == 1)
                {
                    this->issue_shade_command(directolor_tiltOpen, DIRECTOLOR_CODE_ATTEMPTS);
                }
                else
                {
                    if (this->tilt == tilt)
                    {
                        ESP_LOGD("Directolor", "current tilt = requested tilt");
                        return;
                    }
                    if (this->tilt > tilt)
                        this->issue_shade_command(directolor_tiltClose, DIRECTOLOR_CODE_ATTEMPTS);
                    else
                        this->issue_shade_command(directolor_tiltOpen, DIRECTOLOR_CODE_ATTEMPTS);
                    ESP_LOGD(TAG, "current tilt %.2f requested tilt %.2f total seconds for tilt %d", this->tilt, tilt, MS_FOR_FULL_TILT_MOVEMENT);
                    this->ms_duration_for_delayed_stop_ = MS_FOR_FULL_TILT_MOVEMENT * abs(this->tilt - tilt); // gives us the milliseconds of delay.
                    ESP_LOGD(TAG, "desiredDelay: %d", this->ms_duration_for_delayed_stop_);
                    this->start_of_timed_movement_ = millis();
                }
                this->tilt = tilt;
                this->position = 0;
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

        void DirectolorCover::issue_shade_command(BlindAction blind_action, int copies)
        {
            ESP_LOGI(TAG, "Issuing shade command for '%s': action=%s, copies=%d",
                     this->get_name().c_str(), blind_action_to_string(blind_action), copies);
            this->current_action_ = blind_action;
            this->outstanding_send_attempts_ = copies;
        }

        static constexpr uint8_t commandPrototype[] = {0X11, 0X11, 0xC0, 0X10, 0X00, 0X05, 0XBC, 0XFF, 0XFF, 0X8A, 0X91, 0X86, 0X06, 0X99, 0X01, 0X00, 0X8A, 0X91, 0X52, 0X53, 0X00};

        int DirectolorCover::get_radio_command(byte *payload, BlindAction blind_action)
        {
            switch (blind_action)
            {
            case directolor_join:
            case directolor_remove:
                return 0;
            // return getGroupRadioCommand(payload, commandItem);
            case directolor_duplicate:
                return 0;
                // return getDuplicateRadioCommand(payload, commandItem);
            }
            const uint8_t offset = 0;
            int payloadOffset = 0;
            int j = 0;
            while (j + payloadOffset < MAX_PAYLOAD_SIZE)
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
#include "directolor_base.h"
#include <esphome/core/log.h>
#include "esphome.h"
#include <string>

namespace esphome
{
    namespace directolor_base
    {
        static const char *TAG = "directolor_base";

        void DirectolorBase::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Directolor Cover '%s'", this->name_.c_str());
            // ESP_LOGCONFIG("directolor.cover", "  Radio Code: 0x%02X, 0x%02X", this->radio_id_, this->command_);
        }

        void DirectolorBase::loop()
        {
            if (this->outstanding_send_attempts_ > 0)
            {
                BlindAction curr_action = this->current_action_;

                if ((curr_action == directolor_join || curr_action == directolor_remove) && (outstanding_send_attempts_ % 2 == 0))
                {
                    curr_action = directolor_duplicate;
                }

                this->outstanding_send_attempts_--;
                byte payload[MAX_PAYLOAD_SIZE];

                int length = this->get_radio_command(payload, curr_action);

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
        }

        void DirectolorBase::setup()
        {
            ESP_LOGCONFIG(TAG, "Setting up Directolor Cover '%s'", this->get_name().c_str());
            this->command_random_ = random(256);
        }

        void DirectolorBase::issue_shade_command(BlindAction blind_action, int copies)
        {
            ESP_LOGI(TAG, "Issuing shade command for '%s': action=%s, copies=%d",
                     this->get_name().c_str(), blind_action_to_string(blind_action), copies);
            this->current_action_ = blind_action;
            this->outstanding_send_attempts_ = copies;
        }

        static constexpr uint8_t duplicatePrototype[] = {0XFF, 0XFF, 0xC0, 0X12, 0X80, 0X0D, 0x67, 0XFF, 0XFF, 0XC4, 0X05, 0XB1, 0XEC, 0X1D, 0XE3, 0X98, 0x8B, 0X2D, 0XDE, 0X00, 0XEF, 0XC8}; // 6, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23

        int DirectolorBase::get_duplicate_radio_command(byte *payload, BlindAction blind_action)
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

        int DirectolorBase::get_group_radio_command(byte *payload, BlindAction blind_action)
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

        static constexpr uint8_t setFavPrototype[] = {0x0F, 0x00, 0x05, 0x05, 0xFF, 0xFF, 0xB0, 0x51, 0x86, 0x04, 0x2F, 0xB0, 0x51, 0x63, 0x49, 0x00, 0x97, 0x03, 0xAA};

        int DirectolorBase::get_set_fav_radio_command(byte *payload, BlindAction blind_action)
        {
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
                    payload[payloadOffset + j] = setFavPrototype[j];
                    break;
                }
                j++;
            }

            return sizeof(setFavPrototype) + payloadOffset;
        }

        static constexpr uint8_t commandPrototype[] = {0X11, 0X11, 0xC0, 0X10, 0X00, 0X05, 0XBC, 0XFF, 0XFF, 0X8A, 0X91, 0X86, 0X06, 0X99, 0X01, 0X00, 0X8A, 0X91, 0X52, 0X53, 0X00};

        int DirectolorBase::get_radio_command(byte *payload, BlindAction blind_action)
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
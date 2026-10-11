#include "directolor_packet.h"

#include "esphome/core/helpers.h"
#include "esp_random.h"
#include <cstring>

namespace esphome
{
    namespace directolor_radio
    {
        static constexpr uint8_t COMMAND_PROTOTYPE[] = {
            0x11, 0x11, 0xC0, 0x10, 0x00, 0x05, 0xBC, 0xFF, 0xFF, 0x8A, 0x91,
            0x86, 0x06, 0x99, 0x01, 0x00, 0x8A, 0x91, 0x52, 0x53, 0x00};

        static constexpr uint8_t DUPLICATE_PROTOTYPE[] = {
            0xFF, 0xFF, 0xC0, 0x12, 0x80, 0x0D, 0x67, 0xFF, 0xFF, 0xC4, 0x05,
            0xB1, 0xEC, 0x1D, 0xE3, 0x98, 0x8B, 0x2D, 0xDE, 0x00, 0xEF, 0xC8};

        static constexpr uint8_t GROUP_PROTOTYPE[] = {
            0x11, 0x11, 0xC0, 0x0A, 0x40, 0x05, 0x18, 0xFF, 0xFF, 0x8A, 0x91, 0x08, 0x03, 0x01};

        static constexpr uint8_t SET_FAV_PROTOTYPE[] = {
            0x11, 0x11, 0xC0, 0x0F, 0x00, 0x05, 0xD1, 0xFF, 0xFF, 0xB0, 0x51,
            0x86, 0x04, 0xB8, 0xB0, 0x51, 0x63, 0x49, 0x00};

        static int build_duplicate(uint8_t *payload, const uint8_t radio[4], uint8_t &command_random)
        {
            for (size_t j = 0; j < sizeof(DUPLICATE_PROTOTYPE); j++)
            {
                switch (j)
                {
                case 6:
                    payload[j] = command_random++;
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
                    payload[j] = radio[1];
                    break;
                case 18:
                    payload[j] = radio[0];
                    break;
                case 19:
                    payload[j] = radio[2];
                    break;
                case 20:
                    payload[j] = radio[3];
                    break;
                default:
                    payload[j] = DUPLICATE_PROTOTYPE[j];
                    break;
                }
            }
            return static_cast<int>(sizeof(DUPLICATE_PROTOTYPE));
        }

        static int build_group(uint8_t *payload, const uint8_t radio[4], const uint8_t *channels,
                               uint8_t channel_count, BlindAction action, uint8_t &command_random)
        {
            const uint8_t channel = (channels != nullptr && channel_count > 0) ? channels[0] : 0;
            for (size_t j = 0; j < sizeof(GROUP_PROTOTYPE); j++)
            {
                switch (j)
                {
                case 0:
                    payload[j] = radio[0];
                    break;
                case 1:
                    payload[j] = radio[1];
                    break;
                case 6:
                    payload[j] = command_random++;
                    break;
                case 9:
                    payload[j] = radio[2];
                    break;
                case 10:
                    payload[j] = radio[3];
                    break;
                case 12:
                    payload[j] = channel;
                    break;
                case 13:
                    payload[j] = static_cast<uint8_t>(action);
                    break;
                default:
                    payload[j] = GROUP_PROTOTYPE[j];
                    break;
                }
            }
            return static_cast<int>(sizeof(GROUP_PROTOTYPE));
        }

        static int build_set_fav(uint8_t *payload, const uint8_t radio[4], uint8_t &command_random)
        {
            for (size_t j = 0; j < sizeof(SET_FAV_PROTOTYPE); j++)
            {
                switch (j)
                {
                case 0:
                    payload[j] = radio[0];
                    break;
                case 1:
                    payload[j] = radio[1];
                    break;
                case 6:
                    payload[j] = command_random++;
                    break;
                case 9:
                    payload[j] = radio[2];
                    break;
                case 10:
                    payload[j] = radio[3];
                    break;
                case 13:
                    payload[j] = static_cast<uint8_t>(command_random + (esp_random() % 256));
                    break;
                case 14:
                    payload[j] = radio[2];
                    break;
                case 15:
                    payload[j] = radio[3];
                    break;
                default:
                    payload[j] = SET_FAV_PROTOTYPE[j];
                    break;
                }
            }
            return static_cast<int>(sizeof(SET_FAV_PROTOTYPE));
        }

        // Port of Directolor::getRadioCommand. The length byte lives at index 3 and
        // starts at 0x10; each encoded channel increments it. The first channel
        // occupies the prototype's channel slot. Each further channel is inserted
        // immediately after it (payload_offset), which shifts the tail right by
        // channel_count - 1.
        static int build_motion(uint8_t *payload, const uint8_t radio[4], const uint8_t *channels,
                                uint8_t channel_count, BlindAction action, uint8_t &command_random)
        {
            if (channels == nullptr || channel_count == 0)
                return 0;
            if (channel_count > REMOTE_CHANNELS)
                channel_count = REMOTE_CHANNELS;

            int payload_offset = 0;
            for (int j = 0; j < static_cast<int>(sizeof(COMMAND_PROTOTYPE)); j++)
            {
                switch (j)
                {
                case 0:
                    payload[payload_offset + j] = radio[0];
                    break;
                case 1:
                    payload[payload_offset + j] = radio[1];
                    break;
                case 6:
                    payload[payload_offset + j] = command_random++;
                    break;
                case 9:
                    payload[payload_offset + j] = radio[2];
                    break;
                case 10:
                    payload[payload_offset + j] = radio[3];
                    break;
                case 13:
                    payload[payload_offset + j] = static_cast<uint8_t>(command_random + (esp_random() % 256));
                    break;
                case 14:
                    for (uint8_t i = 0; i < channel_count; i++)
                    {
                        payload[j + payload_offset] = channels[i];
                        payload_offset++;
                        payload[3]++;
                    }
                    payload_offset--;
                    break;
                case 16:
                    payload[payload_offset + j] = radio[2];
                    break;
                case 17:
                    payload[payload_offset + j] = radio[3];
                    break;
                case 19:
                    payload[payload_offset + j] = static_cast<uint8_t>(action);
                    break;
                default:
                    payload[payload_offset + j] = COMMAND_PROTOTYPE[j];
                    break;
                }
            }
            return static_cast<int>(sizeof(COMMAND_PROTOTYPE)) + payload_offset;
        }

        int build_on_air_payload(uint8_t out[MAX_NRF_PAYLOAD_SIZE],
                                 const uint8_t radio_code[4],
                                 const uint8_t *channels,
                                 uint8_t channel_count,
                                 BlindAction action,
                                 uint8_t &command_random)
        {
            if (out == nullptr || radio_code == nullptr)
                return 0;

            uint8_t raw[MAX_NRF_PAYLOAD_SIZE];
            std::memset(raw, 0, sizeof(raw));

            int length = 0;
            switch (action)
            {
            case directolor_join:
            case directolor_remove:
                length = build_group(raw, radio_code, channels, channel_count, action, command_random);
                break;
            case directolor_duplicate:
                length = build_duplicate(raw, radio_code, command_random);
                break;
            case directolor_setFav:
                length = build_set_fav(raw, radio_code, command_random);
                break;
            default:
                length = build_motion(raw, radio_code, channels, channel_count, action, command_random);
                break;
            }

            if (length <= 0 || length + 2 > static_cast<int>(MAX_NRF_PAYLOAD_SIZE))
                return 0;

            const uint16_t crc = crc16be(raw, static_cast<uint16_t>(length), 0xFFFF, 0x755b, false, false);
            raw[length++] = static_cast<uint8_t>(crc >> 8);
            raw[length++] = static_cast<uint8_t>(crc & 0xFF);

            const int pad = static_cast<int>(MAX_NRF_PAYLOAD_SIZE) - length;
            std::memset(out, 0x55, MAX_NRF_PAYLOAD_SIZE);
            std::memcpy(out + pad, raw, static_cast<size_t>(length));
            return static_cast<int>(MAX_NRF_PAYLOAD_SIZE);
        }

    } // namespace directolor_radio
} // namespace esphome

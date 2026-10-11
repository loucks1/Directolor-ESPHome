#pragma once
#include <cstdint>

namespace esphome
{
inline uint16_t crc16be(const uint8_t *data, uint16_t len, uint16_t crc = 0, uint16_t poly = 0x1021,
                        bool refin = false, bool refout = false)
{
    if (refin)
        crc ^= 0xffff;
    while (len--)
    {
        crc ^= static_cast<uint16_t>(*data++) << 8;
        for (uint8_t i = 0; i < 8; i++)
        {
            if (crc & 0x8000)
                crc = static_cast<uint16_t>((crc << 1) ^ poly);
            else
                crc = static_cast<uint16_t>(crc << 1);
        }
    }
    return refout ? static_cast<uint16_t>(crc ^ 0xffff) : crc;
}
} // namespace esphome

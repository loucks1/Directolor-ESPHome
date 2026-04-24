#pragma once
#include <cstdint>
#include <cstddef>

enum BlindAction
{
    directolor_open = 0x55,
    directolor_close = 0x44,
    directolor_tiltOpen = 0x52,
    directolor_tiltClose = 0x4C,
    directolor_stop = 0x53,
    directolor_toFav = 0x48,
    directolor_setFav = 0x49,
    directolor_join = 0x01,
    directolor_remove = 0x00,
    directolor_duplicate = 0x51
};

namespace esphome
{
    namespace directolor_radio
    {

        // Centralized Constants
        static constexpr size_t MAX_NRF_PAYLOAD_SIZE = 32;
        static constexpr uint8_t REMOTE_CHANNELS = 6;
        static constexpr uint32_t DEFAULT_TILT_DURATION_MS = 5000;
        // static constexpr size_t MAX_HEX_LOG_BUFFER_SIZE = MAX_NRF_PAYLOAD_SIZE * 3;

    } // namespace directolor_radio
} // namespace esphome
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
        /// Wall-time budget for dense TX bursts per loop() call (ms).
        static constexpr uint32_t TX_BURST_BUDGET_MS = 25;
        /// nRF24 Tpd2stby: oscillator must settle after power-up before TX (ms).
        static constexpr uint32_t TX_POWER_UP_SETTLE_MS = 5;
        /// Ignore duplicate RX of the same action within this window (ms).
        static constexpr uint32_t RX_DEBOUNCE_MS = 500;
        /// Max queued distinct code payloads (each cover multiplies by code_attempts).
        static constexpr size_t PAYLOAD_QUEUE_SIZE = 16;

    } // namespace directolor_radio
} // namespace esphome

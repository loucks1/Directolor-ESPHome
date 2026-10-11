#pragma once

#include "directolor_radio_types.h"
#include <cstdint>

namespace esphome
{
    namespace directolor_radio
    {
        /**
         * Motion commands (open/close/stop/tilt/favorite) can share one radio frame.
         * Join, remove, duplicate, and set-favorite use different frames and are not combined.
         */
        inline bool action_is_mergeable(BlindAction action)
        {
            switch (action)
            {
            case directolor_open:
            case directolor_close:
            case directolor_stop:
            case directolor_tiltOpen:
            case directolor_tiltClose:
            case directolor_toFav:
                return true;
            default:
                return false;
            }
        }

        /**
         * @brief Build the 32-byte buffer written to the nRF24.
         *
         * The frame is right-aligned and padded with leading 0x55. A motion command
         * lists each blind as its own channel byte, inserted where the single-channel
         * prototype holds the channel. Every channel byte increments the length field
         * (prototype base 0x10). Additional channels shift the bytes that follow them.
         * Callers should pass at most REMOTE_CHANNELS (6) channel numbers, in ascending
         * order; extras are not encoded.
         *
         * @return MAX_NRF_PAYLOAD_SIZE on success, 0 on failure
         */
        int build_on_air_payload(uint8_t out[MAX_NRF_PAYLOAD_SIZE],
                                 const uint8_t radio_code[4],
                                 const uint8_t *channels,
                                 uint8_t channel_count,
                                 BlindAction action,
                                 uint8_t &command_random);

    } // namespace directolor_radio
} // namespace esphome

#pragma once
#include <cstdint>
#include <cstddef>

namespace esphome {
namespace directolor_radio {

// Centralized Constants
static constexpr size_t MAX_NRF_PAYLOAD_SIZE = 32;
//static constexpr size_t MAX_HEX_LOG_BUFFER_SIZE = MAX_NRF_PAYLOAD_SIZE * 3;

// Centralized Enums
enum BlindAction : uint8_t {
    DIRECTOLOR_OPEN        = 0x55,
    DIRECTOLOR_CLOSE       = 0x44,
    DIRECTOLOR_TILT_OPEN   = 0x52,
    DIRECTOLOR_TILT_CLOSE  = 0x4C,
    DIRECTOLOR_STOP        = 0x53,
    DIRECTOLOR_TO_FAV      = 0x48,
    DIRECTOLOR_SET_FAV     = 0x49,
    DIRECTOLOR_JOIN        = 0x01,
    DIRECTOLOR_REMOVE      = 0x00,
    DIRECTOLOR_DUPLICATE   = 0x51
};

}  // namespace directolor_radio
}  // namespace esphome
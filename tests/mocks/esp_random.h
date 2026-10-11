#pragma once
#include <cstdint>
#include <stdlib.h>

inline uint32_t esp_random() {
    return rand();
}

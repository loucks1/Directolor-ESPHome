#pragma once
#include <iostream>

#define ESPHOME_LOG_LEVEL_VERBOSE 5
#define ESPHOME_LOG_LEVEL 5

#define ESP_LOGI(tag, format, ...) printf("[I] %s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[W] %s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) printf("[D] %s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) printf("[V] %s: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGCONFIG(tag, format, ...) printf("[C] %s: " format "\n", tag, ##__VA_ARGS__)

inline std::string format_hex_pretty(const uint8_t *data, size_t length) {
    return "hex_pretty_mock";
}
inline const char* format_hex_pretty_to(char* buffer, const uint8_t* data, size_t length) {
    buffer[0] = 'm'; buffer[1] = 'o'; buffer[2] = 'c'; buffer[3] = 'k'; buffer[4] = '\0';
    return buffer;
}

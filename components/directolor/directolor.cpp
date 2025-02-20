#include "esphome/core/log.h"
#include "directolor.h"

namespace esphome {
namespace directolor {

static const char *TAG = "directolor.cover";

void Directolor::setup() {

}

void Directolor::loop() {

}

void Directolor::dump_config() {
    ESP_LOGCONFIG(TAG, "Directolor	cover");
}

cover::CoverTraits Directolor::get_traits() {
    auto traits = cover::CoverTraits();
    traits.set_is_assumed_state(false);
    traits.set_supports_position(false);
    traits.set_supports_tilt(false);

    return traits;
}

void Directolor::control(const cover::CoverCall &call) {
    
}

}  // namespace directolor
}  // namespace esphome

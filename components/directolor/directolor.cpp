#include "esphome/core/log.h"
#include "directolor.h"

namespace esphome {
namespace directolor {

static const char *TAG = "directolor.cover";

void Directolor::setup() {
	pinMode(this->led_pin_, OUTPUT);
	ESP_LOGD("directolor", "setup called");
}

void Directolor::loop() {

}

void Directolor::dump_config() {
    ESP_LOGCONFIG(TAG, "Directolor cover");
}

cover::CoverTraits Directolor::get_traits() {
    auto traits = cover::CoverTraits();
    traits.set_is_assumed_state(true);
    traits.set_supports_position(true);
    traits.set_supports_tilt(true);

    return traits;
}

void Directolor::control(const cover::CoverCall &call) {
  
        this->publish_state(cover::COVER_OPEN); // Update cover state

        // Flash the LED 3 times
        for (int i = 0; i < 3; i++) {
          ESP_LOGD("directolor", "Open called, flashing LED");
          digitalWrite(this->led_pin_, HIGH); // Turn LED on
          delay(200);                         // Wait 200ms
          digitalWrite(this->led_pin_, LOW);  // Turn LED off
          delay(200);                         // Wait 200ms
        }
  }

}  // namespace directolor
}  // namespace esphome

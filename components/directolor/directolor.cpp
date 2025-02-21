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
  if (call.get_position().has_value()) {
       float target = *call.get_position();
       this->current_operation =
           target > this->position ? cover::COVER_OPERATION_OPENING : cover::COVER_OPERATION_CLOSING;

	  ESP_LOGD("directolor", "Open called, flashing LED");
	  ESP_LOGD("directolor", String(target).c_str());
        // Flash the LED 3 times
        for (int i = 0; i < 3; i++) {
          digitalWrite(this->led_pin_, HIGH); // Turn LED on
          delay(200);                         // Wait 200ms
          digitalWrite(this->led_pin_, LOW);  // Turn LED off
          delay(200);                         // Wait 200ms
        }

       this->set_timeout("move", 2000, [this, target]() {
         this->current_operation = cover::COVER_OPERATION_IDLE;
         this->position = target;
         this->publish_state();
       });
     }
     if (call.get_tilt().has_value()) {
       this->tilt = *call.get_tilt();
     }
     if (call.get_stop()) {
       this->cancel_timeout("move");
     }
 
    // this->publish_state();
  }

}  // namespace directolor
}  // namespace esphome

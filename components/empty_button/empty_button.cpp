#include "esphome/core/log.h"
#include "empty_button.h"

namespace esphome {
namespace empty_button {

static const char *TAG = "empty_button.button";

void EmptyButton::setup() {

}

void EmptyButton::dump_config(){
    ESP_LOGCONFIG(TAG, "Empty custom button");
}

} //namespace empty_switch
} //namespace esphome
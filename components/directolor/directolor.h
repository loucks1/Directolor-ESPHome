#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace directolor {

class Directolor : public cover::Cover, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_led_pin(int pin);
  cover::CoverTraits get_traits() override;
  
 private:
   int led_pin_;
  
 protected:
  void control(const cover::CoverCall &call) override;
};

}  // namespace directolor
}  // namespace esphome
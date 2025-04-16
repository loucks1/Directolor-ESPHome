#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace empty_button {

class EmptyButton : public button::Button, public Component {
 public:
  void setup() override;
  void press() override;
  void dump_config() override;
};

} //namespace empty_switch
} //namespace esphome
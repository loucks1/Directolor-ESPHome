#pragma once
#include "esphome/core/component.h"
#include <string>

namespace esphome {
namespace cover {

static constexpr float COVER_OPEN = 1.0f;
static constexpr float COVER_CLOSED = 0.0f;

class CoverTraits {
 public:
  void set_supports_position(bool) {}
  void set_supports_tilt(bool) {}
  void set_supports_stop(bool) {}
  void set_is_assumed_state(bool) {}
};

template <typename T>
class Optional {
 public:
  bool has_value() const { return false; }
  T operator*() const { return T{}; }
};

class CoverCall {
 public:
  Optional<float> get_position() const { return {}; }
  bool get_stop() const { return false; }
  Optional<float> get_tilt() const { return {}; }
};

// Real ESPHome Cover derives from EntityBase, not Component. DirectolorCover
// also inherits Component, so this mock keeps those bases distinct.
class Cover {
 public:
  virtual ~Cover() = default;
  virtual CoverTraits get_traits() = 0;
  virtual void control(const CoverCall &call) = 0;
  void publish_state() {}
  const std::string &get_name() const { return name_; }

  float position{0};
  float tilt{0};
  std::string name_;
};

}  // namespace cover
}  // namespace esphome

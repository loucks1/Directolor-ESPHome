#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
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

class Cover;

/// Saved cover position. apply() publishes and does not call control().
struct CoverRestoreState {
  float position{COVER_OPEN};
  float tilt{COVER_OPEN};
  void apply(Cover *cover);
};

class CoverRestoreResult {
 public:
  bool has_value() const { return has_; }
  CoverRestoreState *operator->() { return &state_; }
  const CoverRestoreState *operator->() const { return &state_; }

  static CoverRestoreResult none() { return {}; }
  static CoverRestoreResult of(const CoverRestoreState &state) {
    CoverRestoreResult result;
    result.has_ = true;
    result.state_ = state;
    return result;
  }

 private:
  bool has_{false};
  CoverRestoreState state_{};
};

// Real ESPHome Cover derives from EntityBase, not Component. DirectolorCover
// also inherits Component, so this mock keeps those bases distinct.
class Cover {
 public:
  virtual ~Cover() = default;
  virtual CoverTraits get_traits() = 0;
  virtual void control(const CoverCall &call) = 0;
  void publish_state() { this->publish_count_++; }

  static void set_test_restore(const CoverRestoreState &state) { test_restore() = CoverRestoreResult::of(state); }
  static void clear_test_restore() { test_restore() = CoverRestoreResult::none(); }

  int publish_count_{0};
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
  // EntityBase::get_name() is deprecated in ESPHome 2026.11.0 and removed in 2027.5.0.
  [[deprecated("Use get_log_name() instead. Will be removed in ESPHome 2027.5.0")]]
#endif
  const std::string &get_name() const { return name_; }
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 11, 0)
  // EntityBase::get_log_name(): pointer for LOG_STR_ARG. On devices this is
  // name_.progmem_ptr(); the host string is already in RAM.
  const LogString *get_log_name() const {
    return reinterpret_cast<const LogString *>(this->name_.c_str());
  }
#endif

  float position{0};
  float tilt{0};
  std::string name_;

 protected:
  // Matches cover::Cover::restore_state_(). The host test injects a value with
  // set_test_restore(); an empty result means "nothing saved".
  CoverRestoreResult restore_state_() {
    CoverRestoreResult result = test_restore();
    test_restore() = CoverRestoreResult::none();
    return result;
  }

 private:
  static CoverRestoreResult &test_restore() {
    static CoverRestoreResult pending;
    return pending;
  }
};

inline void CoverRestoreState::apply(Cover *cover) {
  cover->position = this->position;
  cover->tilt = this->tilt;
  cover->publish_state();
}

}  // namespace cover
}  // namespace esphome

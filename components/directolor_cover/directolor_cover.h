#pragma once
#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "esphome/components/directolor_radio/directolor_radio_types.h"
#include "esphome/components/directolor_radio/directolor_radio.h"

static constexpr uint8_t REMOTE_CHANNELS = 6;
static constexpr uint32_t DEFAULT_TILT_DURATION_MS = 5000;

namespace esphome
{
  namespace directolor_cover
  {
    class DirectolorCover : public cover::Cover, public Component
    {
    public:
      void set_radio_hub(directolor_radio::DirectolorRadio *hub) { this->hub_ = hub; }
      void set_radio_code(const std::vector<uint8_t> &code)
      {
        for (size_t i = 0; i < std::min(code.size(), radio_code_.size()); i++)
        {
          radio_code_[i] = code[i];
        }
      }
      void set_movement_duration(float seconds) { this->movement_duration_ms_ = static_cast<uint32_t>(seconds * 1000.0f); }
      void set_tilt_supported(bool tilt_support) { this->tilt_supported_ = tilt_support; }
      void set_channel(int channel) { this->channel_ = channel; }

      void issue_shade_command(BlindAction blind_action);

      void dump_config() override;
      cover::CoverTraits get_traits() override;
      void control(const cover::CoverCall &call) override;
      void setup() override;
      void loop() override;

    protected:
      directolor_radio::DirectolorRadio *hub_;
      int get_radio_command(uint8_t *payload, BlindAction blind_action);
      int get_group_radio_command(uint8_t *payload, BlindAction blind_action);
      int get_duplicate_radio_command(uint8_t *payload, BlindAction blind_action);
      int get_set_fav_radio_command(uint8_t *payload, BlindAction blind_action);
      std::array<uint8_t, 4> radio_code_{{0x06, 0x04, 0x05, 0x12}};
      uint8_t command_random_;
      uint32_t movement_duration_ms_ = 0;
      bool tilt_supported_ = false;
      uint8_t channel_;

      BlindAction current_action_;
      int8_t outstanding_send_attempts_ = 0;
      float current_position_;

    private:
      void process_outstanding_send_attempts();
    };
  }; // namespace directolor_cover
} // namespace esphome
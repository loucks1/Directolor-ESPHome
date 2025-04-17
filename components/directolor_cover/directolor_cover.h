#pragma once
#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/nrf24l01_base/nrf24l01_base.h" // Include the base component
#include <CRC.h>

#define DIRECTOLOR_REMOTE_CHANNELS 6
#define DIRECTOLOR_CODE_ATTEMPTS 3

namespace esphome
{
  namespace directolor_cover
  {
    enum BlindAction
    {
      directolor_open = 0x55,
      directolor_close = 0x44,
      directolor_tiltOpen = 0x52,
      directolor_tiltClose = 0x4C,
      directolor_stop = 0x53,
      directolor_toFav = 0x48,
      directolor_setFav = 6, // do this one in a bit...
      directolor_join = 0x01,
      directolor_remove = 0x00,
      directolor_duplicate = 4
    };

    class DirectolorCover : public cover::Cover, public Component
    {
    public:
      void set_base(esphome::nrf24l01_base::Nrf24l01_base *base) { this->base_ = base; }
      void set_radio_code(uint8_t code1, uint8_t code2, uint8_t code3, uint8_t code4)
      {
        this->radio_code_[0] = code1;
        this->radio_code_[1] = code2;
        this->radio_code_[2] = code3;
        this->radio_code_[3] = code4;
      }
      void set_movement_duration(float seconds) { this->movement_duration_ms_ = static_cast<uint32_t>(seconds * 1000.0f); }
      void set_tilt_supported(bool tilt_support) { this->tilt_supported_ = tilt_support; }
      void set_channel(int channel) { this->channel_ = channel; }
      void set_favorite_support(bool favorite_enabled) { this->favorite_support_ = favorite_enabled; }

      void dump_config() override;
      cover::CoverTraits get_traits() override;
      void control(const cover::CoverCall &call) override;
      void setup() override;
      void loop() override;

      void sendJoin();

    protected:
      esphome::nrf24l01_base::Nrf24l01_base *base_;
      int get_radio_command(byte *payload, BlindAction blind_action);
      void issue_shade_command(BlindAction blind_action, int copies);
      uint8_t radio_code_[4];
      byte command_random_;
      uint32_t movement_duration_ms_ = 0;
      bool tilt_supported_ = false;
      uint8_t channel_;
      bool favorite_support_ = false;

      BlindAction current_action_;
      int8_t outstanding_send_attempts_ = 0;
      float current_position_;

      unsigned long start_of_timed_movement_;
      int ms_duration_for_delayed_stop_;

      class ActionButton : public button::Button
      {
      public:
        // Updated constructor to accept name and id
        ActionButton(DirectolorCover *parent, const std::string &action) 
            : parent_(parent) {
              this->name = std::string(parent->get_name().c_str()) + " " + action;
              this->id = action + "_" + std::string(parent->get_name().c_str());
              this->set_name(this->name.c_str());
              this->set_object_id(this->id.c_str());
            }
        void press_action() override;

      protected:
        std::string name;
        std::string id;

      private:
        DirectolorCover *parent_;
      };

      ActionButton *duplicate_button_ = nullptr;
      ActionButton *join_button_ = nullptr;
      ActionButton *remove_button_ = nullptr;
      ActionButton *to_fav_button_ = nullptr;
      ActionButton *set_fav_button_ = nullptr;
      void on_action_button_press(std::string &id);
    };

    

  } // namespace directolor_cover
} // namespace esphome
#pragma once
#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include "payload_queue.h"                  // Include the new PayloadQueue class
#include "esphome/components/nrf24/nrf24.h" // Include the base component
#include "esphome/components/nrf24/nRF24L01.h"

#define DIRECTOLOR_REMOTE_CHANNELS 6

#define DIRECTOLOR_CAPTURE_FIRST    // with this enabled, it will only show the first message when in capture mode, otherwise, it dumps every message it can - if you want to see full join or remove codes, you'll need to disable this.
#define DIRECTOLOR_DEBUG_SENT_CODES // with this enabled, we'll log the codes we're sending

#define MS_FOR_FULL_TILT_MOVEMENT 5000

enum BlindAction
{
  directolor_open = 0x55,
  directolor_close = 0x44,
  directolor_tiltOpen = 0x52,
  directolor_tiltClose = 0x4C,
  directolor_stop = 0x53,
  directolor_toFav = 0x48,
  directolor_setFav = 0x49,
  directolor_join = 0x01,
  directolor_remove = 0x00,
  directolor_duplicate = 0x51
};

namespace esphome
{
  namespace directolor_cover
  {
    class DirectolorCover : public cover::Cover, public Component
    {
    public:
      void set_nrf24(nrf24::NRF24Component *parent) { this->radio_ = parent; }
      void set_radio_code(std::vector<uint8_t> code)
      {
        if (code.size() >= 4)
        {
          this->radio_code_[0] = code[0];
          this->radio_code_[1] = code[1];
          this->radio_code_[2] = code[2];
          this->radio_code_[3] = code[3];
        }
      }
      void set_movement_duration(float seconds) { this->movement_duration_ms_ = static_cast<uint32_t>(seconds * 1000.0f); }
      void set_tilt_supported(bool tilt_support) { this->tilt_supported_ = tilt_support; }
      void set_channel(int channel) { this->channel_ = channel; }
      void set_directolor_code_attempts(uint8_t attempts) { this->code_attempts_ = attempts; }
      void set_message_send_repeats(uint16_t repeats) { this->message_send_repeats_ = repeats; }

      void issue_shade_command(BlindAction blind_action);

      void dump_config() override;
      cover::CoverTraits get_traits() override;
      void control(const cover::CoverCall &call) override;
      void setup() override;
      void loop() override;

    protected:
      nrf24::NRF24Component *radio_;
      int get_radio_command(uint8_t *payload, BlindAction blind_action);
      int get_group_radio_command(uint8_t *payload, BlindAction blind_action);
      int get_duplicate_radio_command(uint8_t *payload, BlindAction blind_action);
      int get_set_fav_radio_command(uint8_t *payload, BlindAction blind_action);
      uint8_t radio_code_[4];
      uint8_t command_random_;
      uint32_t movement_duration_ms_ = 0;
      bool tilt_supported_ = false;
      uint8_t channel_;

      BlindAction current_action_;
      int8_t outstanding_send_attempts_ = 0;
      float current_position_;

      unsigned long start_of_timed_movement_;
      int ms_duration_for_delayed_stop_;

    private:
      struct RemoteCode
      {
        uint8_t radioCode[4];
      };
      bool radioStarted();
      bool enterRemoteSearchMode();
      void checkRadioPayload();
      std::string formatHex(char payload[], int start, int count, const char *separator);
      void enterRemoteCaptureMode();
      void send_code();
      void sendPayload(uint8_t *payload);
      void process_outstanding_send_attempts();

      bool radioValid;
      bool radioInitialized;
      RemoteCode remoteCode;
      bool learningRemote;
      unsigned long lastStartAttempt;
      unsigned long currentCooldown = 513;

      uint8_t code_attempts_;
      uint16_t message_send_repeats_;

      PayloadQueue queue_;
      PayloadEntry current_sending_payload_;
    };
  }; // namespace directolor_cover
} // namespace esphome
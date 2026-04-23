#pragma once
#include "esphome/core/component.h"
#include "payload_queue.h"                  // Include the new PayloadQueue class
#include "esphome/components/nrf24/nrf24.h" // Include the base component
#include "esphome/components/nrf24/nRF24L01.h"

static constexpr uint8_t REMOTE_CHANNELS = 6;
static constexpr uint32_t DEFAULT_TILT_DURATION_MS = 5000;

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
  namespace directolor_radio
  {

    class DirectolorRadio : public Component
    {
    public:
      void set_ce_pin(int pin) { ce_pin_ = pin; }
      void set_cs_pin(int pin) { cs_pin_ = pin; }

      void setup() override;
      void loop() override;
      void dump_config() override;
      void set_led_pin(int pin);
      void sendPayload(uint8_t *payload);

    private:
      struct RemoteCode
      {
        uint8_t radioCode[4];
      };

    protected:
      bool radioStarted();
      bool enterRemoteSearchMode();
      void checkRadioPayload();
      std::string formatHex(char payload[], int start, int count, const char *separator);
      void enterRemoteCaptureMode();
      void send_code();

      int ce_pin_;
      int cs_pin_;
      RF24 radio;
      bool radioValid;
      bool radioInitialized;
      RemoteCode remoteCode;
      bool learningRemote;
      unsigned long lastMillis;
      unsigned long lastStartAttempt;
      unsigned long currentCooldown = 513;

      PayloadQueue queue_;
      PayloadEntry current_sending_payload_;
    };

  } // namespace directolor
} // namespace esphome
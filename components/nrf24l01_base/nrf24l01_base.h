#pragma once

#include "esphome/core/component.h"
#include <SPI.h>
#include <RF24.h>
#include <stdint.h>
#include "payload_queue.h" // Include the new PayloadQueue class

#define MAX_PAYLOAD_SIZE 32 // maximum payload that you can send with the nRF24l01+

#define MESSAGE_SEND_RETRIES 513/2     // the number of times to resend the message(seems like numbers > 400 are more reliable - feel free to change as necessary)

#define DIRECTOLOR_CAPTURE_FIRST    // with this enabled, it will only show the first message when in capture mode, otherwise, it dumps every message it can - if you want to see full join or remove codes, you'll need to disable this.
#define DIRECTOLOR_DEBUG_SENT_CODES // with this enabled, we'll log the codes we're sending

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
  namespace nrf24l01_base
  {

    class Nrf24l01_base : public Component
    {
    public:
      void set_ce_pin(int pin) { ce_pin_ = pin; }
      void set_cs_pin(int pin) { cs_pin_ = pin; }

      void setup() override;
      void loop() override;
      void dump_config() override;
      void set_led_pin(int pin);
      void sendPayload(byte *payload);

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

      PayloadQueue queue_;
      PayloadEntry current_sending_payload_;
    };

  } // namespace directolor
} // namespace esphome

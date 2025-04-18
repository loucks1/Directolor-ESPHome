#pragma once

#include "esphome/core/component.h"
#include <SPI.h>
#include <RF24.h>
#include <stdint.h>
#include "payload_queue.h" // Include the new PayloadQueue class

#define MAX_QUEUED_COMMANDS 20

#define COMMAND_CODE_LENGTH 17
#define DUPLICATE_CODE_LENGTH 18
#define GROUP_CODE_LENGTH 10
#define STORE_FAV_CODE_LENGTH 15

#define MAX_PAYLOAD_SIZE 32 // maximum payload that you can send with the nRF24l01+

#define MESSAGE_SEND_ATTEMPTS 3         // this is the number of times we will generate and send the message (3 seems to work well for me, but feel free to change up or down as needed)
#define MESSAGE_SEND_RETRIES 513        // the number of times to resend the message(seems like numbers > 400 are more reliable - feel free to change as necessary)
#define INTERMESSAGE_SEND_DELAY 512 / 3 // the delay between message sends (if this is too low, the blinds seem to 'miss' messsages)

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
  directolor_setFav = 6, // do this one in a bit...
  directolor_join = 0x01,
  directolor_remove = 0x00,
  directolor_duplicate = 4
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

      uint8_t storeFavPrototype[25] = {0x55, 0x55, 0x55, 0X11, 0X11, 0xC0, 0x0F, 0x00, 0x05, 0x2B, 0xFF, 0xFF, 0xBB, 0x0D, 0x86, 0x04, 0x20, 0xBB, 0x0D, 0x63, 0x49, 0x00, 0xC4, 0x10, 0XAA};

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

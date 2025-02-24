#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"
#include <SPI.h>
#include <RF24.h>
#include <stdint.h>

#define DIRECTOLOR_MAX_QUEUED_COMMANDS 20

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

namespace esphome {
namespace directolor {

class Directolor : public cover::Cover, public Component {
 public:
  void set_pin(int pin) { led_pin_ = pin; }
  void set_ce_pin(int pin) { ce_pin_ = pin; }
  void set_cs_pin(int pin) { cs_pin_ = pin; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_led_pin(int pin);
  cover::CoverTraits get_traits() override;

 private:
  struct CommandItem
    {
        uint8_t *radioCodes;
        uint8_t channels;
        BlindAction blindAction;
        uint8_t resendRemainingCount;
    };

   struct RemoteCode
    {
        uint8_t radioCode[4];
    };

    uint8_t storeFavPrototype[25] = {0x55, 0x55, 0x55, 0X11, 0X11, 0xC0, 0x0F, 0x00, 0x05, 0x2B, 0xFF, 0xFF, 0xBB, 0x0D, 0x86, 0x04, 0x20, 0xBB, 0x0D, 0x63, 0x49, 0x00, 0xC4, 0x10, 0XAA};

   static const uint8_t matchPattern[4]; // this is what we use to find out what the codes for the remote are

   static RF24 radio;
   static bool messageIsSending;
   static bool learningRemote;
   static bool radioValid;
   static RemoteCode remoteCode;
   static unsigned long lastMillis;
   static short lastCommand;
   static uint16_t _cepin;
   static uint16_t _cspin;
   static uint32_t _spi_speed;
   static bool radioInitialized;
   CommandItem commandItems[DIRECTOLOR_MAX_QUEUED_COMMANDS];
  
 protected:
  void control(const cover::CoverCall &call) override;

  int led_pin_;
  int ce_pin_;
  int cs_pin_;
};

}  // namespace directolor
}  // namespace esphome

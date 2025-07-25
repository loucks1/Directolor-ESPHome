#include "esphome/core/log.h"
#include "nrf24l01_base.h"

namespace esphome
{
  namespace nrf24l01_base
  {

    static const char *TAG = "nrf24l01_base";
    static const uint8_t MATCHPATTERN[4] = {0xC0, 0X11, 0X00, 0X05}; // this is what we use to find out the codes for a new remote

    void Nrf24l01_base::setup()
    {
      ESP_LOGI(TAG, "setup called");

      this->enterRemoteSearchMode();
      this->current_sending_payload_.send_attempts = 0;
    }

    bool Nrf24l01_base::radioStarted()
    {
      if (this->radio.failureDetected)
      {
          this->radioValid = false;
          this->radio.failureDetected = false;
      }

      if (!this->radioValid)
      {
        if (!radioInitialized)
        {
          this->radio = RF24(this->ce_pin_, this->cs_pin_, RF24_SPI_SPEED);
          this->radioInitialized = true;
        }

        ESP_LOGI(TAG, "attempting to start radio");

        this->radioValid = this->radio.begin();

        if (this->radioValid)
        {
          this->radio.setAutoAck(false);               // auto-ack has to be off or everything breaks because I haven't been able to RE the protocol CRC / validation
          this->radio.setCRCLength(RF24_CRC_DISABLED); // disable CRC

          this->radio.setChannel(53); // remotes transmit at 2453 mHz

          this->radio.closeReadingPipe(0); // close pipes in case something left it listening
          this->radio.closeReadingPipe(1);
          this->radio.closeReadingPipe(2);
          this->radio.closeReadingPipe(3);
          this->radio.closeReadingPipe(4);
          this->radio.closeReadingPipe(5);
          ESP_LOGI(TAG, "radio started");
        }
        else
        {
          ESP_LOGE(TAG, "Failure starting radio");
        }
      }
      
      return this->radioValid;
    }

    bool Nrf24l01_base::enterRemoteSearchMode()
    {
      if (this->radioStarted())
      {
        this->radio.stopListening();
        this->radio.setAddressWidth(5);
        this->radio.openReadingPipe(1, 0x5555555555); // always uses pipe 0
        ESP_LOGI(TAG, "searching for remote");
        this->radio.startListening(); // put radio in RX mode
        this->learningRemote = true;
        this->radio.setPayloadSize(MAX_PAYLOAD_SIZE);
        memset(&this->remoteCode, 0, sizeof(remoteCode));
        return true;
      }
      return false;
    }

    void Nrf24l01_base::enterRemoteCaptureMode()
    {
      if (this->remoteCode.radioCode[0] || this->remoteCode.radioCode[1])
      {
        this->radio.stopListening();
        this->radio.setAddressWidth(3);

        union
        {
          uint32_t address;
          uint8_t bytes[4];
        } combined;

        combined.bytes[2] = this->remoteCode.radioCode[0];
        combined.bytes[1] = this->remoteCode.radioCode[1];
        combined.bytes[0] = 0xC0;

        this->radio.openReadingPipe(1, combined.address); // always uses pipe 0
        this->radio.openReadingPipe(0, 0xFFFFC0);

        this->radio.startListening(); // put radio in RX mode
        ESP_LOGI(TAG, "listening for remote codes");
      }
      else if (learningRemote)
      {
        enterRemoteSearchMode();
      }
      else
      {
        radio.powerDown();
      }
    }

    void Nrf24l01_base::loop()
    {
      this->send_code();
      this->checkRadioPayload();
    }

    std::string Nrf24l01_base::formatHex(char payload[], int start, int count, const char *separator)
    {
      std::string result = "";
      for (int i = start; i < count + start; i++)
      {
        // Add separator if not the first element and separator isn't a space
        if (i != start && strcmp(separator, " ") != 0)
        {
          result += ", ";
        }
        result += separator;
        // Convert the byte to a two-digit hex string with leading zero if needed
        char hex[3]; // Buffer for two hex digits plus null terminator
        snprintf(hex, sizeof(hex), "%02X", (unsigned char)payload[i]);
        result += hex;
      }
      return result;
    }

    void Nrf24l01_base::checkRadioPayload() // could modify this to allow capture if commands were sent using multiple channels
    {
      if (this->radioStarted())
      {
        uint8_t pipe;
        if (this->radio.available(&pipe))
        {
          char payload[32];
          uint8_t bytes = this->radio.getPayloadSize(); // get the size of the payload
          this->radio.read(&payload, bytes);            // fetch payload from FIFO

          if (this->learningRemote)
          {
            ESP_LOGV(TAG, "checking payload for match: %s", this->formatHex(payload, 0, bytes, " ").c_str());

            uint8_t foundPattern = 0;

            for (int i = 0; i < bytes; i++)
            {
              if (payload[i] != MATCHPATTERN[foundPattern])
                foundPattern = 0;
              else
                foundPattern++;

              if (foundPattern > 3 && i > 4)
              {
                this->remoteCode.radioCode[0] = payload[i - 5];
                this->remoteCode.radioCode[1] = payload[i - 4];
                this->remoteCode.radioCode[2] = payload[i + 4];
                this->remoteCode.radioCode[3] = payload[i + 5];
                this->learningRemote = false;
                this->enterRemoteCaptureMode();
                ESP_LOGI(TAG, "Found Remote with address: %s", this->formatHex((char *)this->remoteCode.radioCode, 0, 4, " ").c_str());
              }
            }
          }
          else
          {
            bytes = payload[0] + 4;
            if (bytes > 32)
              return; // invalid payload - discarding
#ifdef DIRECTOLOR_CAPTURE_FIRST
            bool skip = (millis() - this->lastMillis < 25);
            this->lastMillis = millis();
            if (skip)
              return;
#endif

            const char *command = "ERROR";

            if (payload[4] == 0xFF && payload[5] == 0xFF && payload[6] == this->remoteCode.radioCode[2] && payload[7] == this->remoteCode.radioCode[3] && payload[8] == 0x86)
              switch (payload[bytes - 5])
              {
              case directolor_open:
                command = "Open";
                break;
              case directolor_close:
                command = "Close";
                break;
              case directolor_tiltOpen:
                command = "Tilt Open";
                break;
              case directolor_tiltClose:
                command = "Tilt Close";
                break;
              case directolor_stop:
                command = "Stop";
                break;
              case directolor_toFav:
                command = "to Fav";
                break;
              case directolor_setFav:
                command = "Store Favorite";
              }

            if (payload[4] == 0xFF && payload[5] == 0xFF && payload[6] == this->remoteCode.radioCode[2] && payload[7] == this->remoteCode.radioCode[3] && payload[8] == 0x08)
            {
              switch (payload[bytes - 4])
              {
              case directolor_join:
                command = "Join";
                break;
              case directolor_remove:
                command = "Remove";
                break;
              }
            }

            if (payload[0] == 0x12 && payload[1] == 0x80 && payload[2] == 0x0D && payload[4] == 0xFF && payload[5] == 0xFF && payload[16] == this->remoteCode.radioCode[2] && payload[17] == this->remoteCode.radioCode[3] && payload[18] == 0xC8)
              command = "Duplicate";

            if (command == "ERROR")
              return;

            ESP_LOGV(TAG, "bytes: %d pipe: %d: %s", bytes - 1, pipe, this->formatHex(payload, 0, bytes, " ").c_str());
            ESP_LOGI(TAG, "Received %s from: %s", command, this->formatHex((char *)this->remoteCode.radioCode, 0, 4, " ").c_str());
          }
        }
      }
    }

    void Nrf24l01_base::sendPayload(uint8_t *payload)
    {
      this->queue_.enqueue(payload, MESSAGE_SEND_RETRIES);
    }

    void Nrf24l01_base::send_code()
    {
      if (!this->radioStarted())
        return;
      if (this->current_sending_payload_.send_attempts == 0)
      {
        if (queue_.dequeue(this->current_sending_payload_))
        {
          ESP_LOGV(TAG, "Processing - send_attempts: %d: %s", this->current_sending_payload_.send_attempts, this->formatHex((char *)this->current_sending_payload_.payload, 0, 32, " ").c_str());

          this->radio.powerUp();
          this->radio.stopListening(); // put radio in TX mode
          this->radio.setPALevel(RF24_PA_MAX);
          this->radio.setAddressWidth(3);
          this->radio.enableDynamicAck();
          this->radio.openWritingPipe(0x060406);
          this->radio.setPayloadSize(MAX_PAYLOAD_SIZE);
        }
      }
      else
      {
        ESP_LOGV(TAG, "sending code (attempts left: %d)", this->current_sending_payload_.send_attempts);

        unsigned long startMillis = millis();
        while (--this->current_sending_payload_.send_attempts > 0)
        {
          this->radio.writeFast(this->current_sending_payload_.payload, MAX_PAYLOAD_SIZE, true); // we aren't waiting for an ACK, so we need to writeFast with multiCast set to true
          if (this->current_sending_payload_.send_attempts % 3 == 0)
          {
            this->radio.txStandBy();
            if (millis() - startMillis > 25)
              break;
          }
        }
        this->radio.txStandBy();
        if (this->current_sending_payload_.send_attempts == 0)
        {
          ESP_LOGV(TAG, "send code complete");
          if (!queue_.dequeue(this->current_sending_payload_))
            this->enterRemoteCaptureMode(); // go back and power down
        }
      }
    }

    void Nrf24l01_base::dump_config()
    {
      ESP_LOGCONFIG(TAG, "nrf24l01_base cover");
    }

  } // namespace nrf24l01_base
} // namespace esphome

#include "directolor_radio.h"

#include <esphome/core/log.h>
#include "esphome.h"
#include "esp_random.h"
#include <string>

namespace esphome
{
    namespace directolor_radio
    {
        static const char *TAG = "directolor_radio";
        static const uint8_t MATCHPATTERN[4] = {0xC0, 0X11, 0X00, 0X05}; // this is what we use to find out the codes for a new remote

        void DirectolorRadio::setup()
        {
            this->current_sending_payload_.send_attempts = 0;
            this->radioValid = false;
        }

        void DirectolorRadio::loop()
        {
            this->checkRadioPayload();
            this->send_code();
        }

        void DirectolorRadio::dump_config()
        {
            this->radio_->dump_config();
        }

        bool DirectolorRadio::radioStarted()
        {
            //     if (this->radio_->failureDetected)
            //     {
            //         this->radioValid = false;
            //         this->radio_->failureDetected = false;
            //     }

            if (!this->radioValid)
            {
                if (millis() - this->lastStartAttempt < currentCooldown)
                    return false; // don't try to restart too often

                if (currentCooldown < 60000)
                    currentCooldown *= 2; // back off up to 1 minute

                lastStartAttempt = millis();

                ESP_LOGI(TAG, "attempting to start radio");

                this->radioValid = this->radio_->begin();

                if (this->radioValid)
                {
                    this->radio_->stop_listening();                          // make sure we're not still in listening mode from a previous session
                    this->radio_->setAutoAck(false);                         // auto-ack has to be off or everything breaks because I haven't been able to RE the protocol CRC / validation
                    this->radio_->setCRCLength(nRF24L01::RF24_CRC_DISABLED); // disable CRC

                    ESP_LOGI(TAG, "radio started");
                    this->enterRemoteSearchMode();
                }
                else
                {
                    ESP_LOGE(TAG, "Failure starting radio");
                }
            }

            return this->radioValid;
        }

        bool DirectolorRadio::enterRemoteSearchMode()
        {
            if (this->radioStarted())
            {
                ESP_LOGI(TAG, "attempting to start listening");
                this->radio_->stop_listening();
                this->radio_->set_address_width(5);
                this->radio_->open_reading_pipe(1, 0x5555555555); // always uses pipe 0
                ESP_LOGI(TAG, "searching for remote");
                this->radio_->start_listening(); // put radio in RX mode
                this->learningRemote = true;
                ESP_LOGI(TAG, "started listening - press a button on your remote");
                if (esp_log_get_default_level() >= ESP_LOG_VERBOSE)
                {
                    this->dump_config();
                }
                return true;
            }
            return false;
        }

        void DirectolorRadio::enterRemoteCaptureMode()
        {
            if (this->sniffed_remote_code_[0] || this->sniffed_remote_code_[1])
            {
                this->radio_->stop_listening();
                this->radio_->set_address_width(3);

                uint8_t capture_addr[3];
                capture_addr[2] = this->sniffed_remote_code_[0];
                capture_addr[1] = this->sniffed_remote_code_[1];
                capture_addr[0] = 0xC0;
                this->radio_->open_reading_pipe(1, capture_addr);

                this->radio_->start_listening(); // put radio in RX mode
                ESP_LOGI(TAG, "Now listening for 3-byte payloads on address: [%02X, %02X, %02X]",
                         capture_addr[0], capture_addr[1], capture_addr[2]);
            }
            else if (learningRemote)
            {
                enterRemoteSearchMode();
            }
            else
            {
                this->radio_->powerDown();
            }
        }

        void DirectolorRadio::sendPayload(uint8_t *payload)
        {
            this->queue_.enqueue(payload, this->message_send_repeats_);
        }

        void DirectolorRadio::send_code()
        {
            if (!this->radioStarted())
                return;
            if (this->current_sending_payload_.send_attempts == 0)
            {
                if (queue_.dequeue(this->current_sending_payload_))
                {
                    ESP_LOGV(TAG, "Processing - send_attempts: %d: %s",
                             this->current_sending_payload_.send_attempts,
                             format_hex_pretty(this->current_sending_payload_.payload, esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE).c_str());

                    this->radio_->powerUp();
                    this->radio_->stop_listening(); // put radio in TX mode
                    this->radio_->set_address_width(3);
                    this->radio_->open_writing_pipe(0x060406);
                    //                    this->radio_->set_payload_size(MAX_PAYLOAD_SIZE);  //TOdo; remove
                }
            }
            else
            {
                ESP_LOGV(TAG, "sending code (attempts left: %d)", this->current_sending_payload_.send_attempts);

                unsigned long startMillis = millis();
                while (--this->current_sending_payload_.send_attempts > 0)
                {
                    this->radio_->writeFast(this->current_sending_payload_.payload, this->radio_->getPayloadSize(), true); // we aren't waiting for an ACK, so we need to writeFast with multiCast set to true
                    if (this->current_sending_payload_.send_attempts % 3 == 0)
                    {
                        this->radio_->txStandBy();
                        if (millis() - startMillis > 25)
                            break;
                    }
                }
                this->radio_->txStandBy();
                if (this->current_sending_payload_.send_attempts == 0)
                {
                    ESP_LOGV(TAG, "send code complete");
                    if (!queue_.dequeue(this->current_sending_payload_))
                        this->enterRemoteCaptureMode(); // go back and power down
                }
            }
        }

        void DirectolorRadio::checkRadioPayload() // could modify this to allow capture if commands were sent using multiple channels
        {
            if (this->radioStarted())
            {
                uint8_t pipe;
                if (this->radio_->available(&pipe))
                {
                    uint8_t bytes = this->radio_->getPayloadSize(); // get the size of the payload
                    char payload[bytes];
                    this->radio_->read(payload, bytes); // fetch payload from FIFO

                    if (this->learningRemote)
                    {
                        ESP_LOGV(TAG, "checking payload for match: %s", format_hex_pretty(reinterpret_cast<const uint8_t *>(payload), bytes).c_str());

                        uint8_t foundPattern = 0;

                        for (int i = 0; i < bytes; i++)
                        {
                            if (payload[i] != MATCHPATTERN[foundPattern])
                                foundPattern = 0;
                            else
                                foundPattern++;

                            if (foundPattern > 3 && i > 4)
                            {
                                this->sniffed_remote_code_[0] = payload[i - 5];
                                this->sniffed_remote_code_[1] = payload[i - 4];
                                this->sniffed_remote_code_[2] = payload[i + 4];
                                this->sniffed_remote_code_[3] = payload[i + 5];
                                this->learningRemote = false;
                                ESP_LOGI(TAG, "Found Remote with address: [0x%02X, 0x%02X, 0x%02X, 0x%02X]",
                                         this->sniffed_remote_code_[0], this->sniffed_remote_code_[1], this->sniffed_remote_code_[2], this->sniffed_remote_code_[3]);
                                this->enterRemoteCaptureMode();
                            }
                        }
                    }
                    else
                    {
                        bytes = payload[0] + 4;
                        if (bytes > esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE)
                            return; // invalid payload - discarding

                        if (esp_log_get_default_level() <= ESP_LOG_DEBUG)
                        {
                            bool skip = (millis() - this->lastMillis_ < 25);
                            this->lastMillis_ = millis();
                            if (skip)
                                return;
                        }

                        const char *command = "ERROR";

                        if (payload[4] == 0xFF && payload[5] == 0xFF && payload[6] == this->sniffed_remote_code_[2] && payload[7] == this->sniffed_remote_code_[3] && payload[8] == 0x86)
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

                        if (payload[4] == 0xFF && payload[5] == 0xFF && payload[6] == this->sniffed_remote_code_[2] && payload[7] == this->sniffed_remote_code_[3] && payload[8] == 0x08)
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

                        if (payload[0] == 0x12 && payload[1] == 0x80 && payload[2] == 0x0D && payload[4] == 0xFF && payload[5] == 0xFF && payload[16] == this->sniffed_remote_code_[2] && payload[17] == this->sniffed_remote_code_[3] && payload[18] == 0xC8)
                            command = "Duplicate";

                        if (command == "ERROR")
                            return;

                        ESP_LOGV(TAG, "bytes: %d pipe: %d: %s", bytes, pipe, format_hex_pretty(reinterpret_cast<const uint8_t *>(payload), bytes).c_str());
                        char buffer[12];
                        ESP_LOGI(TAG, "Received %s from: %s", command, format_hex_pretty_to(buffer, this->sniffed_remote_code_.data(), this->sniffed_remote_code_.size()));
                    }
                }
            }
        }

    }
}
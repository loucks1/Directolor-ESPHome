#include "directolor_cover.h"
#include "esphome/components/directolor_radio/directolor_radio.h"
#include <esphome/core/log.h>
#include "esphome.h"
#include <cmath>

namespace esphome
{
    namespace directolor_cover
    {
        static const char *TAG = "directolor_cover";

        void DirectolorCover::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Directolor Cover '%s'", this->name_.c_str());
            ESP_LOGCONFIG(TAG, "  Radio Code: %s", format_hex_pretty(radio_code_.data(), radio_code_.size()).c_str());
            ESP_LOGCONFIG(TAG, "  Movement Duration: %.2f seconds", this->movement_duration_ms_ / 1000.0f);
            ESP_LOGCONFIG(TAG, "  Tilt Supported: %s", this->tilt_supported_ ? "Yes" : "No");
            ESP_LOGCONFIG(TAG, "  Channel: %d", this->channel_);
        }

        cover::CoverTraits DirectolorCover::get_traits()
        {
            auto traits = cover::CoverTraits();
            traits.set_supports_position(this->movement_duration_ms_ != 0);
            traits.set_supports_tilt(this->tilt_supported_);
            traits.set_supports_stop(true);
            traits.set_is_assumed_state(true);
            return traits;
        }

        void DirectolorCover::setup()
        {
            ESP_LOGCONFIG(TAG, "Setting up Directolor Cover '%s'", this->get_name().c_str());
        }

        void DirectolorCover::control(const cover::CoverCall &call)
        {
            // Kill any existing timer immediately when a new command arrives
            this->cancel_timeout("delayed_stop");

            if (call.get_position().has_value())
            {
                float pos = *call.get_position();

                if (pos == cover::COVER_OPEN)
                {
                    this->issue_shade_command(directolor_open);
                }
                else if (pos == cover::COVER_CLOSED)
                {
                    this->issue_shade_command(directolor_close);
                }
                else if (pos == this->position)
                {
                    ESP_LOGI(TAG, "Shade '%s' already at requested position.", this->get_name().c_str());
                }
                else
                {
                    if (this->position > pos)
                        this->issue_shade_command(directolor_close);
                    else
                        this->issue_shade_command(directolor_open);

                    uint32_t delay = static_cast<uint32_t>(this->movement_duration_ms_ * std::abs(this->position - pos));

                    this->set_timeout("delayed_stop", delay, [this, pos]()
                                      {
                                        this->issue_shade_command(directolor_stop);
                                        ESP_LOGD(TAG, "Scheduled stop executed for %s", this->get_name().c_str()); });
                    ESP_LOGD(TAG, "%s scheduled for stop after delay. Position: %.2f, Target: %.2f, Delay: %lu ms", this->get_name().c_str(), this->position, pos, (unsigned long)delay);
                }

                this->position = pos;
                if (this->tilt_supported_)
                    this->tilt = 0;
                this->publish_state();
            }

            if (call.get_stop())
            {
                this->issue_shade_command(directolor_stop);
                this->publish_state();
            }

            if (call.get_tilt().has_value())
            {
                float tilt_val = *call.get_tilt();

                if (tilt_val == 0.0f)
                {
                    this->issue_shade_command(directolor_tiltClose);
                }
                else if (tilt_val == 1.0f)
                {
                    this->issue_shade_command(directolor_tiltOpen);
                }
                else
                {
                    if (this->tilt == tilt_val)
                    {
                        ESP_LOGD(TAG, "Current tilt matches requested tilt.");
                        return;
                    }

                    if (this->tilt > tilt_val)
                        this->issue_shade_command(directolor_tiltClose);
                    else
                        this->issue_shade_command(directolor_tiltOpen);

                    uint32_t delay = static_cast<uint32_t>(esphome::directolor_radio::DEFAULT_TILT_DURATION_MS * std::abs(this->tilt - tilt_val));

                    this->set_timeout("delayed_stop", delay, [this]()
                                      {
                                        this->issue_shade_command(directolor_stop);
                                        ESP_LOGD(TAG, "Tilt scheduled stop executed for %s", this->get_name().c_str()); });
                    ESP_LOGD(TAG, "%s tilt scheduled for stop after delay. Current: %.2f, Target: %.2f, Delay: %lu ms", this->get_name().c_str(), this->tilt, tilt_val, (unsigned long)delay);
                }
                this->tilt = tilt_val;
                this->publish_state();
            }
        }

        void DirectolorCover::issue_shade_command(BlindAction blind_action)
        {
            if (this->hub_ == nullptr)
            {
                ESP_LOGE(TAG, "No radio hub for '%s'", this->get_name().c_str());
                return;
            }

            ESP_LOGI(TAG, "Issuing shade command for '%s': action=%s", this->get_name().c_str(), this->hub_->blind_action_to_string(blind_action));

            // Join/remove are one-shot and always followed by a duplicate pairing frame.
            // Repeating them can re-trigger programming on the blind, and their frames
            // are not multi-channel motion packets, so they are not merged.
            if (blind_action == directolor_join || blind_action == directolor_remove)
            {
                if (!this->hub_->submit_command(this->radio_code_.data(), this->channel_, blind_action, 1))
                    ESP_LOGW(TAG, "Failed to enqueue command for '%s' (queue full)", this->get_name().c_str());
                if (!this->hub_->submit_command(this->radio_code_.data(), this->channel_, directolor_duplicate, 1))
                    ESP_LOGW(TAG, "Failed to enqueue command for '%s' (queue full)", this->get_name().c_str());
                return;
            }

            // One logical request. The hub emits code_attempts distinct payloads and
            // folds a later same-action channel into this request instead of queueing
            // another transmission.
            const uint8_t attempts = this->hub_->get_code_attempts();
            if (!this->hub_->submit_command(this->radio_code_.data(), this->channel_, blind_action, attempts))
                ESP_LOGW(TAG, "Failed to enqueue command for '%s' (queue full)", this->get_name().c_str());
        }

    } // namespace directolor_cover
} // namespace esphome

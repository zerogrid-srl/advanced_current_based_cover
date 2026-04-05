#include "advanced_current_based_cover.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/number/number.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <cfloat>

namespace esphome {
namespace advanced_current_based {

static const char *const TAG = "advanced_current_based.cover";

using namespace esphome::cover;

CoverTraits AdvancedCurrentBasedCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_toggle(true);
  traits.set_is_assumed_state(false);
  return traits;
}

void AdvancedCurrentBasedCover::control(const CoverCall &call) {
  // Don't allow control during calibration
  if (this->calibration_state_ != CALIBRATION_IDLE) {
    ESP_LOGW(TAG, "'%s' - Cannot control cover during calibration", this->name_.c_str());
    return;
  }

  if (call.get_stop()) {
    this->direction_idle_();
  }
  if (call.get_toggle().has_value()) {
    if (this->current_operation != COVER_OPERATION_IDLE) {
      this->start_direction_(COVER_OPERATION_IDLE);
      this->publish_state();
    } else {
      if (this->position == COVER_CLOSED || this->last_operation_ == COVER_OPERATION_CLOSING) {
        this->target_position_ = COVER_OPEN;
        this->start_direction_(COVER_OPERATION_OPENING);
      } else {
        this->target_position_ = COVER_CLOSED;
        this->start_direction_(COVER_OPERATION_CLOSING);
      }
    }
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (fabsf(this->position - pos) < 0.01) {
      // already at target
    } else {
      auto op = pos < this->position ? COVER_OPERATION_CLOSING : COVER_OPERATION_OPENING;
      this->target_position_ = pos;
      this->start_direction_(op);
    }
  }
}

void AdvancedCurrentBasedCover::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    this->position = 0.5f;
  }

  // Load calibration data from preferences
  if (this->save_calibration_) {
    this->pref_ = global_preferences->make_preference<CalibrationData>(this->get_object_id_hash());
    this->load_calibration_data_();
  }

  // Initialize calibration status sensor
  if (this->calibration_data_.is_calibrated) {
    this->update_calibration_status_("Calibration completed");
  } else {
    this->update_calibration_status_("Idle");
  }

  // Start auto-calibration if enabled and not calibrated
  if (this->auto_calibration_on_boot_ && !this->calibration_data_.is_calibrated) {
    this->set_timeout("auto_calibration", 2000, [this]() {
      ESP_LOGI(TAG, "'%s' - Starting auto-calibration on boot", this->name_.c_str());
      this->start_calibration();
    });
  }
}

void AdvancedCurrentBasedCover::loop() {
  // Handle calibration loop
  if (this->calibration_state_ != CALIBRATION_IDLE) {
    this->calibration_loop_();
    return;
  }

  // Normal operation loop
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  const uint32_t now = millis();

  if (this->current_operation == COVER_OPERATION_OPENING) {
    if (this->malfunction_detection_ && this->is_initial_delay_finished_() && this->is_closing_malfunction_()) {  // Malfunction
      this->direction_idle_();
      this->malfunction_trigger_->trigger();
      ESP_LOGI(TAG, "'%s' - [OPEN] STOPPED: Malfunction detected - Current flow in close circuit (threshold: %.2fA)",
               this->name_.c_str(), this->close_moving_current_threshold_);
    } else if (this->is_opening_blocked_()) {  // Blocked
      ESP_LOGI(TAG, "'%s' - [OPEN] STOPPED: Obstacle detected at %.2fA (threshold: %.2fA)",
               this->name_.c_str(), this->open_sensor_->get_state(), this->open_obstacle_current_threshold_);
      this->direction_idle_();
      if (this->obstacle_rollback_ != 0) {
        this->set_timeout("rollback", 300, [this]() {
          ESP_LOGV(TAG, "'%s' - Rollback.", this->name_.c_str());
          this->target_position_ = clamp(this->position - this->obstacle_rollback_, 0.0F, 1.0F);
          this->start_direction_(COVER_OPERATION_CLOSING);
        });
      }
    } else if (this->is_initial_delay_finished_() && !this->is_opening_()) {  // End reached
      auto dur = (now - this->start_dir_time_) / 1e3f;
      ESP_LOGI(TAG, "'%s' - [OPEN] STOPPED: Endstop reached at %.2fA (threshold: %.2fA) - Duration: %.1fs",
               this->name_.c_str(), this->open_sensor_->get_state(), this->open_endstop_current_threshold_, dur);
      this->direction_idle_(COVER_OPEN);
    }
  } else if (this->current_operation == COVER_OPERATION_CLOSING) {
    if (this->malfunction_detection_ && this->is_initial_delay_finished_() && this->is_opening_malfunction_()) {  // Malfunction
      this->direction_idle_();
      this->malfunction_trigger_->trigger();
      ESP_LOGI(TAG, "'%s' - [CLOSE] STOPPED: Malfunction detected - Current flow in open circuit (threshold: %.2fA)",
               this->name_.c_str(), this->open_moving_current_threshold_);
    } else if (this->is_closing_blocked_()) {  // Blocked
      ESP_LOGI(TAG, "'%s' - [CLOSE] STOPPED: Obstacle detected at %.2fA (threshold: %.2fA)",
               this->name_.c_str(), this->close_sensor_->get_state(), this->close_obstacle_current_threshold_);
      this->direction_idle_();
      if (this->obstacle_rollback_ != 0) {
        this->set_timeout("rollback", 300, [this]() {
          ESP_LOGV(TAG, "'%s' - Rollback.", this->name_.c_str());
          this->target_position_ = clamp(this->position + this->obstacle_rollback_, 0.0F, 1.0F);
          this->start_direction_(COVER_OPERATION_OPENING);
        });
      }
    } else if (this->is_initial_delay_finished_() && !this->is_closing_()) {  // End reached
      auto dur = (now - this->start_dir_time_) / 1e3f;
      ESP_LOGI(TAG, "'%s' - [CLOSE] STOPPED: Endstop reached at %.2fA (threshold: %.2fA) - Duration: %.1fs",
               this->name_.c_str(), this->close_sensor_->get_state(), this->close_endstop_current_threshold_, dur);
      this->direction_idle_(COVER_CLOSED);
    }
  }

  // Calculate effective timeout
  uint32_t effective_timeout;
  if (this->max_duration_ != UINT32_MAX) {
    // Use absolute max_duration if configured (backward compatibility)
    effective_timeout = this->max_duration_;
  } else {
    // Use calibrated duration + margin
    uint32_t calibrated_dur = (this->current_operation == COVER_OPERATION_OPENING)
                              ? this->get_open_duration_()
                              : this->get_close_duration_();
    effective_timeout = calibrated_dur + this->timeout_margin_;
  }

  if (now - this->start_dir_time_ > effective_timeout) {
    auto op_str = (this->current_operation == COVER_OPERATION_OPENING) ? "OPEN" : "CLOSE";
    ESP_LOGI(TAG, "'%s' - [%s] STOPPED: Timeout reached (effective: %dms, current: %.2fA)",
             this->name_.c_str(), op_str, effective_timeout,
             (this->current_operation == COVER_OPERATION_OPENING) ?
               this->open_sensor_->get_state() : this->close_sensor_->get_state());
    this->direction_idle_();
  }

  // Recompute position every loop cycle
  this->recompute_position_();

  if (this->current_operation != COVER_OPERATION_IDLE && this->is_at_target_()) {
    this->direction_idle_();
  }

  // Send current position every 3 seconds (reduced MQTT traffic)
  if (this->current_operation != COVER_OPERATION_IDLE && now - this->last_publish_time_ > 3000) {
    this->publish_state(false);
    this->update_position_sensor_();
    this->last_publish_time_ = now;
  }
}

void AdvancedCurrentBasedCover::calibration_loop_() {
  const uint32_t now = millis();

  switch (this->calibration_state_) {
    case CALIBRATION_OPENING:
      // Wait 3500ms for soft-starter before starting current readings
      if (now - this->calibration_start_time_ < 3500) {
        return;
      }

      // Don't check endstop before minimum calibration time (10s) to avoid false detections
      if (now - this->calibration_start_time_ < 10000) {
        // Just update reading time, don't check current
        if (now - this->last_calibration_reading_ >= 500) {
          this->last_calibration_reading_ = now;
        }
        return;
      }

      // Read current every 500ms
      if (now - this->last_calibration_reading_ >= 500) {
        this->last_calibration_reading_ = now;
        float current = this->open_sensor_->get_state();

        ESP_LOGI(TAG, "'%s' - [CALIB-OPEN] Current reading: %.2fA (threshold: 0.10A, confirm: %d/3)",
                 this->name_.c_str(), current, this->endstop_confirmations_);

        // Check if current indicates endstop (use dynamic threshold if available)
        float calibration_threshold = this->get_dynamic_threshold_(this->calibration_endstop_threshold_,
                                                                  this->calibration_endstop_threshold_number_);
        if (current < calibration_threshold) {
          this->endstop_confirmations_++;
          ESP_LOGI(TAG, "'%s' - [CALIB-OPEN] Endstop threshold CROSSED at %.2fA (confirmation %d/3)",
                   this->name_.c_str(), current, this->endstop_confirmations_);

          // Require 3 consecutive confirmations (1.5s total)
          if (this->endstop_confirmations_ >= 3) {
            this->calibration_data_.open_duration = now - this->calibration_start_time_;
            ESP_LOGI(TAG, "'%s' - Open endstop CONFIRMED at %.2fA: duration=%dms (%.1fs)",
                     this->name_.c_str(), current, this->calibration_data_.open_duration,
                     this->calibration_data_.open_duration / 1000.0f);

            // Stop motor
            this->stop_prev_trigger_();
            this->stop_trigger_->trigger();
            this->prev_command_trigger_ = this->stop_trigger_;

            // Save to number entity
            if (this->open_duration_number_ != nullptr) {
              auto call = this->open_duration_number_->make_call();
              call.set_value(this->calibration_data_.open_duration / 1000.0f);
              call.perform();
            }

            // Transition to wait state
            this->calibration_state_ = CALIBRATION_OPENING_WAIT;
            this->calibration_start_time_ = now;
            this->last_calibration_reading_ = 0;
            this->endstop_confirmations_ = 0;
          }
        } else {
          // Current above threshold, reset confirmation counter
          this->endstop_confirmations_ = 0;
        }
      }

      // Safety timeout
      if (now - this->calibration_start_time_ > this->max_duration_) {
        ESP_LOGE(TAG, "'%s' - Calibration failed: timeout during opening", this->name_.c_str());
        this->update_calibration_status_("Failed: timeout");
        this->calibration_state_ = CALIBRATION_FAILED;
        this->stop_prev_trigger_();
        this->stop_trigger_->trigger();
        this->calibration_failed_trigger_->trigger();
      }
      break;

    case CALIBRATION_OPENING_WAIT:
      // Wait 2500ms between opening and closing
      if (now - this->calibration_start_time_ >= 2500) {
        ESP_LOGI(TAG, "'%s' - Starting close calibration...", this->name_.c_str());
        this->update_calibration_status_("Calibrating - Closing...");
        this->calibration_state_ = CALIBRATION_CLOSING;
        this->calibration_start_time_ = millis();
        this->last_calibration_reading_ = 0;

        // Start closing
        this->stop_prev_trigger_();
        this->close_trigger_->trigger();
        this->prev_command_trigger_ = this->close_trigger_;
      }
      break;

    case CALIBRATION_CLOSING:
      // Wait 3500ms for soft-starter before starting current readings
      if (now - this->calibration_start_time_ < 3500) {
        return;
      }

      // Don't check endstop before minimum calibration time (10s) to avoid false detections
      if (now - this->calibration_start_time_ < 10000) {
        // Just update reading time, don't check current
        if (now - this->last_calibration_reading_ >= 500) {
          this->last_calibration_reading_ = now;
        }
        return;
      }

      // Read current every 500ms
      if (now - this->last_calibration_reading_ >= 500) {
        this->last_calibration_reading_ = now;
        float current = this->close_sensor_->get_state();

        ESP_LOGI(TAG, "'%s' - [CALIB-CLOSE] Current reading: %.2fA (threshold: 0.10A, confirm: %d/3)",
                 this->name_.c_str(), current, this->endstop_confirmations_);

        // Check if current indicates endstop (use dynamic threshold if available)
        float calibration_threshold = this->get_dynamic_threshold_(this->calibration_endstop_threshold_,
                                                                  this->calibration_endstop_threshold_number_);
        if (current < calibration_threshold) {
          this->endstop_confirmations_++;
          ESP_LOGI(TAG, "'%s' - [CALIB-CLOSE] Endstop threshold CROSSED at %.2fA (confirmation %d/3)",
                   this->name_.c_str(), current, this->endstop_confirmations_);

          // Require 3 consecutive confirmations (1.5s total)
          if (this->endstop_confirmations_ >= 3) {
            this->calibration_data_.close_duration = now - this->calibration_start_time_;
            ESP_LOGI(TAG, "'%s' - Close endstop CONFIRMED at %.2fA: duration=%dms (%.1fs)",
                     this->name_.c_str(), current, this->calibration_data_.close_duration,
                     this->calibration_data_.close_duration / 1000.0f);

            // Stop motor
            this->stop_prev_trigger_();
            this->stop_trigger_->trigger();
            this->prev_command_trigger_ = this->stop_trigger_;

            // Save to number entity
            if (this->close_duration_number_ != nullptr) {
              auto call = this->close_duration_number_->make_call();
              call.set_value(this->calibration_data_.close_duration / 1000.0f);
              call.perform();
            }

            // Check if calibration is valid: durations != 0s and != 30s
            uint32_t open_sec = this->calibration_data_.open_duration / 1000;
            uint32_t close_sec = this->calibration_data_.close_duration / 1000;

            if (open_sec != 30 && open_sec != 0 && close_sec != 30 && close_sec != 0) {
              // Calibration successful
              this->calibration_data_.is_calibrated = true;
              this->open_duration_ = this->calibration_data_.open_duration;
              this->close_duration_ = this->calibration_data_.close_duration;

              ESP_LOGI(TAG, "'%s' - Calibration COMPLETE! Open: %.1fs, Close: %.1fs",
                       this->name_.c_str(), open_sec, close_sec);
              this->update_calibration_status_("Calibration Complete");
              this->calibration_complete_trigger_->trigger();

              // Reset to idle and set position to closed
              this->calibration_state_ = CALIBRATION_IDLE;
              this->position = COVER_CLOSED;
              this->publish_state();
              this->endstop_confirmations_ = 0;
            } else {
              // Calibration failed: invalid durations
              ESP_LOGE(TAG, "'%s' - Calibration FAILED: invalid durations (open=%ds, close=%ds)",
                       this->name_.c_str(), open_sec, close_sec);
              this->update_calibration_status_("Failed: invalid durations");
              this->calibration_state_ = CALIBRATION_FAILED;
              this->calibration_failed_trigger_->trigger();
              this->endstop_confirmations_ = 0;
            }
          }
        } else {
          // Current above threshold, reset confirmation counter
          this->endstop_confirmations_ = 0;
        }
      }

      // Safety timeout
      if (now - this->calibration_start_time_ > this->max_duration_) {
        ESP_LOGE(TAG, "'%s' - Calibration failed: timeout during closing", this->name_.c_str());
        this->update_calibration_status_("Failed: timeout");
        this->calibration_state_ = CALIBRATION_FAILED;
        this->stop_prev_trigger_();
        this->stop_trigger_->trigger();
        this->calibration_failed_trigger_->trigger();
      }
      break;

    case CALIBRATION_FAILED:
      // Reset to idle after 1 second
      this->set_timeout("calibration_failed", 1000, [this]() {
        this->calibration_state_ = CALIBRATION_IDLE;
        this->update_calibration_status_("Idle");
      });
      break;

    default:
      break;
  }
}

void AdvancedCurrentBasedCover::start_calibration() {
  float calib_threshold = this->get_dynamic_threshold_(this->calibration_endstop_threshold_,
                                                      this->calibration_endstop_threshold_number_);
  ESP_LOGI(TAG, "'%s' - Starting calibration (endstop threshold: %.2fA)", this->name_.c_str(), calib_threshold);

  this->update_calibration_status_("Calibrating - Opening...");
  this->calibration_state_ = CALIBRATION_OPENING;
  this->calibration_start_time_ = millis();
  this->last_calibration_reading_ = 0;

  // Start opening
  this->stop_prev_trigger_();
  this->open_trigger_->trigger();
  this->prev_command_trigger_ = this->open_trigger_;
}

void AdvancedCurrentBasedCover::save_calibration_data_() {
  if (this->pref_.save(&this->calibration_data_)) {
    ESP_LOGI(TAG, "'%s' - Calibration data saved to flash", this->name_.c_str());
  } else {
    ESP_LOGW(TAG, "'%s' - Failed to save calibration data to flash", this->name_.c_str());
  }

  // Also save to number entities if available (convert milliseconds to seconds)
  if (this->open_duration_number_ != nullptr) {
    float open_seconds = this->calibration_data_.open_duration / 1000.0f;
    auto call = this->open_duration_number_->make_call();
    call.set_value(open_seconds);
    call.perform();
    ESP_LOGI(TAG, "'%s' - Saved open duration to number entity: %.1fs", this->name_.c_str(), open_seconds);
  }

  if (this->close_duration_number_ != nullptr) {
    float close_seconds = this->calibration_data_.close_duration / 1000.0f;
    auto call = this->close_duration_number_->make_call();
    call.set_value(close_seconds);
    call.perform();
    ESP_LOGI(TAG, "'%s' - Saved close duration to number entity: %.1fs", this->name_.c_str(), close_seconds);
  }
}

void AdvancedCurrentBasedCover::load_calibration_data_() {
  if (this->pref_.load(&this->calibration_data_)) {
    if (this->calibration_data_.is_calibrated) {
      this->open_duration_ = this->calibration_data_.open_duration;
      this->close_duration_ = this->calibration_data_.close_duration;
      ESP_LOGI(TAG, "'%s' - Loaded calibration data: Open=%dms, Close=%dms", this->name_.c_str(), this->open_duration_,
               this->close_duration_);
    } else {
      ESP_LOGI(TAG, "'%s' - No calibration data found", this->name_.c_str());
    }
  } else {
    ESP_LOGV(TAG, "'%s' - Failed to load calibration data", this->name_.c_str());
  }
}

void AdvancedCurrentBasedCover::direction_idle_(float new_position) {
  this->start_direction_(COVER_OPERATION_IDLE);
  if (new_position != FLT_MAX) {
    this->position = new_position;
  }
  this->publish_state();
  this->update_position_sensor_();
}

void AdvancedCurrentBasedCover::dump_config() {
  LOG_COVER("", "Advanced Current Based Cover", this);
  LOG_SENSOR("  ", "Open Sensor", this->open_sensor_);
  ESP_LOGCONFIG(TAG, "  Open moving current threshold: %.11fA", this->open_moving_current_threshold_);
  if (this->open_obstacle_current_threshold_ != FLT_MAX) {
    ESP_LOGCONFIG(TAG, "  Open obstacle current threshold: %.11fA", this->open_obstacle_current_threshold_);
  }
  if (this->open_endstop_current_threshold_ != FLT_MAX) {
    ESP_LOGCONFIG(TAG, "  Open endstop current threshold: %.11fA", this->open_endstop_current_threshold_);
  }
  ESP_LOGCONFIG(TAG, "  Open Duration: %.1fs", this->open_duration_ / 1e3f);
  LOG_SENSOR("  ", "Close Sensor", this->close_sensor_);
  ESP_LOGCONFIG(TAG, "  Close moving current threshold: %.11fA", this->close_moving_current_threshold_);
  if (this->close_obstacle_current_threshold_ != FLT_MAX) {
    ESP_LOGCONFIG(TAG, "  Close obstacle current threshold: %.11fA", this->close_obstacle_current_threshold_);
  }
  if (this->close_endstop_current_threshold_ != FLT_MAX) {
    ESP_LOGCONFIG(TAG, "  Close endstop current threshold: %.11fA", this->close_endstop_current_threshold_);
  }
  ESP_LOGCONFIG(TAG, "  Close Duration: %.1fs", this->close_duration_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Obstacle Rollback: %.1f%%", this->obstacle_rollback_ * 100);
  if (this->max_duration_ != UINT32_MAX) {
    ESP_LOGCONFIG(TAG, "  Maximum duration: %.1fs", this->max_duration_ / 1e3f);
  }
  ESP_LOGCONFIG(TAG, "  Start sensing delay: %.1fs", this->start_sensing_delay_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Startup delay (soft-starter): %.1fs", this->startup_delay_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Malfunction detection: %s", YESNO(this->malfunction_detection_));
  ESP_LOGCONFIG(TAG, "  Auto calibration on boot: %s", YESNO(this->auto_calibration_on_boot_));
  ESP_LOGCONFIG(TAG, "  Save calibration: %s", YESNO(this->save_calibration_));
  ESP_LOGCONFIG(TAG, "  Endstop detection time: %.1fs", this->endstop_detection_time_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Calibrated: %s", YESNO(this->calibration_data_.is_calibrated));
}

float AdvancedCurrentBasedCover::get_setup_priority() const { return setup_priority::DATA; }

void AdvancedCurrentBasedCover::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}

// Helper function to get current threshold value from Number entity or fallback to fixed value
float AdvancedCurrentBasedCover::get_dynamic_threshold_(float fixed_value, number::Number *number_entity) const {
  if (number_entity != nullptr) {
    float state = number_entity->state;
    if (!std::isnan(state)) {
      return state;
    }
  }
  return fixed_value;
}

bool AdvancedCurrentBasedCover::is_opening_() const {
  float threshold = this->get_dynamic_threshold_(this->open_moving_current_threshold_,
                                                 this->open_moving_current_threshold_number_);
  return this->open_sensor_->get_state() > threshold;
}

bool AdvancedCurrentBasedCover::is_opening_blocked_() const {
  float threshold = this->get_dynamic_threshold_(this->open_obstacle_current_threshold_,
                                               this->open_obstacle_current_threshold_number_);
  if (threshold == FLT_MAX) {
    return false;
  }
  return this->open_sensor_->get_state() > threshold;
}

bool AdvancedCurrentBasedCover::is_at_open_endstop_() const {
  // Use moving threshold as fallback if endstop threshold not configured
  float threshold = (this->open_endstop_current_threshold_ == FLT_MAX)
                    ? this->get_dynamic_threshold_(this->open_moving_current_threshold_,
                                                   this->open_moving_current_threshold_number_)
                    : this->get_dynamic_threshold_(this->open_endstop_current_threshold_,
                                                   this->open_endstop_current_threshold_number_);
  return this->open_sensor_->get_state() <= threshold;
}

bool AdvancedCurrentBasedCover::is_closing_() const {
  float threshold = this->get_dynamic_threshold_(this->close_moving_current_threshold_,
                                                this->close_moving_current_threshold_number_);
  return this->close_sensor_->get_state() > threshold;
}

bool AdvancedCurrentBasedCover::is_closing_blocked_() const {
  float threshold = this->get_dynamic_threshold_(this->close_obstacle_current_threshold_,
                                               this->close_obstacle_current_threshold_number_);
  if (threshold == FLT_MAX) {
    return false;
  }
  return this->close_sensor_->get_state() > threshold;
}

bool AdvancedCurrentBasedCover::is_at_close_endstop_() const {
  // Use moving threshold as fallback if endstop threshold not configured
  float threshold = (this->close_endstop_current_threshold_ == FLT_MAX)
                    ? this->get_dynamic_threshold_(this->close_moving_current_threshold_,
                                                   this->close_moving_current_threshold_number_)
                    : this->get_dynamic_threshold_(this->close_endstop_current_threshold_,
                                                   this->close_endstop_current_threshold_number_);
  return this->close_sensor_->get_state() <= threshold;
}

bool AdvancedCurrentBasedCover::is_opening_malfunction_() const {
  // Use higher threshold (0.50A) for malfunction detection to avoid false positives from magnetic coupling
  return this->open_sensor_->get_state() > 0.50f;
}

bool AdvancedCurrentBasedCover::is_closing_malfunction_() const {
  // Use higher threshold (0.50A) for malfunction detection to avoid false positives from magnetic coupling
  return this->close_sensor_->get_state() > 0.50f;
}

bool AdvancedCurrentBasedCover::is_initial_delay_finished_() const {
  // Total delay = startup_delay (soft-starter) + start_sensing_delay (stabilization)
  uint32_t total_delay = this->startup_delay_ + this->start_sensing_delay_;
  return millis() - this->start_dir_time_ > total_delay;
}

bool AdvancedCurrentBasedCover::is_at_target_() const {
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      if (this->target_position_ == COVER_OPEN) {
        if (!this->is_initial_delay_finished_())  // During initial delay, state is assumed
          return false;
        return !this->is_opening_();
      }
      // For partial movements: ensure minimum time has passed to prevent false target reach
      if (this->position >= this->target_position_) {
        uint32_t elapsed = millis() - this->start_dir_time_;
        float expected_time = (this->target_position_ - (this->position - 0.01f)) * this->get_open_duration_();
        // Require at least 70% of expected time to have passed (protection against loop delays)
        if (elapsed < expected_time * 0.7f) {
          return false;  // Target position reached too quickly, likely due to loop delay
        }
        return true;
      }
      return false;
    case COVER_OPERATION_CLOSING:
      if (this->target_position_ == COVER_CLOSED) {
        if (!this->is_initial_delay_finished_())  // During initial delay, state is assumed
          return false;
        return !this->is_closing_();
      }
      // For partial movements: ensure minimum time has passed to prevent false target reach
      if (this->position <= this->target_position_) {
        uint32_t elapsed = millis() - this->start_dir_time_;
        float expected_time = ((this->position + 0.01f) - this->target_position_) * this->get_close_duration_();
        // Require at least 70% of expected time to have passed (protection against loop delays)
        if (elapsed < expected_time * 0.7f) {
          return false;  // Target position reached too quickly, likely due to loop delay
        }
        return true;
      }
      return false;
    case COVER_OPERATION_IDLE:
    default:
      return true;
  }
}

void AdvancedCurrentBasedCover::start_direction_(CoverOperation dir) {
  if (dir == this->current_operation)
    return;

  this->recompute_position_();
  Trigger<> *trig;
  switch (dir) {
    case COVER_OPERATION_IDLE:
      trig = this->stop_trigger_;
      break;
    case COVER_OPERATION_OPENING:
      this->last_operation_ = dir;
      trig = this->open_trigger_;
      break;
    case COVER_OPERATION_CLOSING:
      this->last_operation_ = dir;
      trig = this->close_trigger_;
      break;
    default:
      return;
  }

  this->current_operation = dir;

  this->stop_prev_trigger_();
  trig->trigger();
  this->prev_command_trigger_ = trig;

  const auto now = millis();
  this->start_dir_time_ = now;
  this->last_recompute_time_ = now;
}

void AdvancedCurrentBasedCover::recompute_position_() {
  if (this->current_operation == COVER_OPERATION_IDLE)
    return;

  const uint32_t now = millis();
  const uint32_t elapsed = now - this->start_dir_time_;

  // During motor startup phase (soft-starter), no actual movement occurs yet
  if (elapsed <= this->startup_delay_) {
    return;  // Don't update position or last_recompute_time during startup
  }

  float dir;
  float action_dur;
  switch (this->current_operation) {
    case COVER_OPERATION_OPENING:
      dir = 1.0F;
      action_dur = this->get_open_duration_();  // Use helper method for dynamic duration
      break;
    case COVER_OPERATION_CLOSING:
      dir = -1.0F;
      action_dur = this->get_close_duration_();  // Use helper method for dynamic duration
      break;
    default:
      return;
  }

  // First recompute after startup: adjust last_recompute_time to skip startup delay
  if (this->last_recompute_time_ == this->start_dir_time_) {
    this->last_recompute_time_ = this->start_dir_time_ + this->startup_delay_;
  }

  // Calculate time delta - PROTECT AGAINST LONG LOOP CYCLES
  uint32_t delta_time = now - this->last_recompute_time_;

  // If loop was blocked for >500ms, limit delta to prevent position jumps
  // This prevents desynchronization when ESP32 is overloaded
  if (delta_time > 500) {
    ESP_LOGW(TAG, "'%s' - Loop cycle took %dms! Limiting position update to 500ms to prevent jump",
             this->name_.c_str(), delta_time);
    delta_time = 500;
  }

  // Calculate position change only for actual movement time (after startup)
  this->position += dir * delta_time / action_dur;
  this->position = clamp(this->position, 0.0F, 1.0F);

  this->last_recompute_time_ = now;
}

void AdvancedCurrentBasedCover::update_position_sensor_() {
  if (this->position_sensor_ != nullptr) {
    float percentage = this->position * 100.0F;
    this->position_sensor_->publish_state(percentage);
  }
}

void AdvancedCurrentBasedCover::update_calibration_status_(const std::string &status) {
  if (this->calibration_status_sensor_ != nullptr) {
    this->calibration_status_sensor_->publish_state(status);
  }
}

uint32_t AdvancedCurrentBasedCover::get_open_duration_() const {
  // If number entity is available and has state, use it (convert seconds to milliseconds)
  if (this->open_duration_number_ != nullptr && this->open_duration_number_->has_state()) {
    return static_cast<uint32_t>(this->open_duration_number_->state * 1000);
  }
  // Otherwise use the fixed/calibrated duration
  return this->open_duration_;
}

uint32_t AdvancedCurrentBasedCover::get_close_duration_() const {
  // If number entity is available and has state, use it (convert seconds to milliseconds)
  if (this->close_duration_number_ != nullptr && this->close_duration_number_->has_state()) {
    return static_cast<uint32_t>(this->close_duration_number_->state * 1000);
  }
  // Otherwise use the fixed/calibrated duration
  return this->close_duration_;
}

void AdvancedCurrentBasedCover::force_open() {
  ESP_LOGI(TAG, "'%s' - Force open triggered - current position: %.2f", this->name_.c_str(), this->position);

  // Stop any calibration in progress
  if (this->calibration_state_ != CALIBRATION_IDLE) {
    this->calibration_state_ = CALIBRATION_IDLE;
    this->update_calibration_status_("Cancelled - Force open triggered");
  }

  // Set position to fully closed so the cover will run for full duration
  this->position = COVER_CLOSED;
  this->target_position_ = COVER_OPEN;
  this->start_direction_(COVER_OPERATION_OPENING);
  this->publish_state();
  this->update_position_sensor_();
}

void AdvancedCurrentBasedCover::force_close() {
  ESP_LOGI(TAG, "'%s' - Force close triggered - current position: %.2f", this->name_.c_str(), this->position);

  // Stop any calibration in progress
  if (this->calibration_state_ != CALIBRATION_IDLE) {
    this->calibration_state_ = CALIBRATION_IDLE;
    this->update_calibration_status_("Cancelled - Force close triggered");
  }

  // Set position to fully open so the cover will run for full duration
  this->position = COVER_OPEN;
  this->target_position_ = COVER_CLOSED;
  this->start_direction_(COVER_OPERATION_CLOSING);
  this->publish_state();
  this->update_position_sensor_();
}

}  // namespace advanced_current_based
}  // namespace esphome

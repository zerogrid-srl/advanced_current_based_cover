#pragma once

#include "esphome/components/cover/cover.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include <cfloat>

namespace esphome {
namespace advanced_current_based {

enum CalibrationState : uint8_t {
  CALIBRATION_IDLE = 0,
  CALIBRATION_OPENING,
  CALIBRATION_OPENING_WAIT,
  CALIBRATION_CLOSING,
  CALIBRATION_CLOSING_WAIT,
  CALIBRATION_COMPLETE,
  CALIBRATION_FAILED,
};

struct CalibrationData {
  uint32_t open_duration;
  uint32_t close_duration;
  bool is_calibrated;
};

class AdvancedCurrentBasedCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  Trigger<> *get_stop_trigger() const { return this->stop_trigger_; }

  Trigger<> *get_open_trigger() const { return this->open_trigger_; }
  void set_position_sensor(sensor::Sensor *position_sensor) { this->position_sensor_ = position_sensor; }
  void set_open_sensor(sensor::Sensor *open_sensor) { this->open_sensor_ = open_sensor; }
  void set_open_moving_current_threshold(float open_moving_current_threshold) {
    this->open_moving_current_threshold_ = open_moving_current_threshold;
  }
  void set_open_obstacle_current_threshold(float open_obstacle_current_threshold) {
    this->open_obstacle_current_threshold_ = open_obstacle_current_threshold;
  }
  void set_open_endstop_current_threshold(float open_endstop_current_threshold) {
    this->open_endstop_current_threshold_ = open_endstop_current_threshold;
  }
  void set_open_moving_current_threshold_number(number::Number *open_moving_current_threshold_number) {
    this->open_moving_current_threshold_number_ = open_moving_current_threshold_number;
  }
  void set_open_obstacle_current_threshold_number(number::Number *open_obstacle_current_threshold_number) {
    this->open_obstacle_current_threshold_number_ = open_obstacle_current_threshold_number;
  }
  void set_open_endstop_current_threshold_number(number::Number *open_endstop_current_threshold_number) {
    this->open_endstop_current_threshold_number_ = open_endstop_current_threshold_number;
  }
  void set_open_duration(uint32_t open_duration) { this->open_duration_ = open_duration; }

  Trigger<> *get_close_trigger() const { return this->close_trigger_; }
  void set_close_sensor(sensor::Sensor *close_sensor) { this->close_sensor_ = close_sensor; }
  void set_close_moving_current_threshold(float close_moving_current_threshold) {
    this->close_moving_current_threshold_ = close_moving_current_threshold;
  }
  void set_close_obstacle_current_threshold(float close_obstacle_current_threshold) {
    this->close_obstacle_current_threshold_ = close_obstacle_current_threshold;
  }
  void set_close_endstop_current_threshold(float close_endstop_current_threshold) {
    this->close_endstop_current_threshold_ = close_endstop_current_threshold;
  }
  void set_close_moving_current_threshold_number(number::Number *close_moving_current_threshold_number) {
    this->close_moving_current_threshold_number_ = close_moving_current_threshold_number;
  }
  void set_close_obstacle_current_threshold_number(number::Number *close_obstacle_current_threshold_number) {
    this->close_obstacle_current_threshold_number_ = close_obstacle_current_threshold_number;
  }
  void set_close_endstop_current_threshold_number(number::Number *close_endstop_current_threshold_number) {
    this->close_endstop_current_threshold_number_ = close_endstop_current_threshold_number;
  }
  void set_close_duration(uint32_t close_duration) { this->close_duration_ = close_duration; }

  void set_max_duration(uint32_t max_duration) { this->max_duration_ = max_duration; }
  void set_timeout_margin(uint32_t timeout_margin) { this->timeout_margin_ = timeout_margin; }
  void set_obstacle_rollback(float obstacle_rollback) { this->obstacle_rollback_ = obstacle_rollback; }

  void set_malfunction_detection(bool malfunction_detection) { this->malfunction_detection_ = malfunction_detection; }
  void set_start_sensing_delay(uint32_t start_sensing_delay) { this->start_sensing_delay_ = start_sensing_delay; }
  void set_startup_delay(uint32_t startup_delay) { this->startup_delay_ = startup_delay; }

  Trigger<> *get_malfunction_trigger() const { return this->malfunction_trigger_; }

  // Advanced calibration features
  void set_auto_calibration_on_boot(bool auto_calibration) { this->auto_calibration_on_boot_ = auto_calibration; }
  void set_save_calibration(bool save_calibration) { this->save_calibration_ = save_calibration; }
  void set_endstop_detection_time(uint32_t endstop_detection_time) {
    this->endstop_detection_time_ = endstop_detection_time;
  }
  void set_calibration_endstop_threshold(float calibration_endstop_threshold) {
    this->calibration_endstop_threshold_ = calibration_endstop_threshold;
  }
  void set_calibration_endstop_threshold_number(number::Number *calibration_endstop_threshold_number) {
    this->calibration_endstop_threshold_number_ = calibration_endstop_threshold_number;
  }

  Trigger<> *get_calibration_complete_trigger() const { return this->calibration_complete_trigger_; }
  Trigger<> *get_calibration_failed_trigger() const { return this->calibration_failed_trigger_; }

  void start_calibration();
  bool is_calibrated() const { return this->calibration_data_.is_calibrated; }

  // Force open/close methods
  void force_open();
  void force_close();

  // Setters for number entities (dynamic durations)
  void set_open_duration_number(number::Number *open_duration_number) {
    this->open_duration_number_ = open_duration_number;
  }
  void set_close_duration_number(number::Number *close_duration_number) {
    this->close_duration_number_ = close_duration_number;
  }

  // Setter for calibration status sensor
  void set_calibration_status_sensor(text_sensor::TextSensor *calibration_status_sensor) {
    this->calibration_status_sensor_ = calibration_status_sensor;
  }

  cover::CoverTraits get_traits() override;

 protected:
  void control(const cover::CoverCall &call) override;
  void stop_prev_trigger_();

  bool is_at_target_() const;
  // Helper for dynamic threshold reading
  float get_dynamic_threshold_(float fixed_value, number::Number *number_entity) const;

  bool is_opening_() const;
  bool is_opening_blocked_() const;
  bool is_at_open_endstop_() const;
  bool is_closing_() const;
  bool is_closing_blocked_() const;
  bool is_at_close_endstop_() const;
  bool is_initial_delay_finished_() const;
  bool is_opening_malfunction_() const;
  bool is_closing_malfunction_() const;

  void direction_idle_(float new_position = FLT_MAX);
  void start_direction_(cover::CoverOperation dir);

  void recompute_position_();
  void update_position_sensor_();
  void update_calibration_status_(const std::string &status);

  // Helper methods to get durations (from number entities if available, otherwise fixed values)
  uint32_t get_open_duration_() const;
  uint32_t get_close_duration_() const;

  // Calibration methods
  void calibration_loop_();
  void save_calibration_data_();
  void load_calibration_data_();

  Trigger<> *stop_trigger_{new Trigger<>()};

  sensor::Sensor *position_sensor_{nullptr};
  sensor::Sensor *open_sensor_{nullptr};
  Trigger<> *open_trigger_{new Trigger<>()};
  float open_moving_current_threshold_;
  float open_obstacle_current_threshold_{FLT_MAX};
  float open_endstop_current_threshold_{FLT_MAX};
  number::Number *open_moving_current_threshold_number_{nullptr};
  number::Number *open_obstacle_current_threshold_number_{nullptr};
  number::Number *open_endstop_current_threshold_number_{nullptr};
  uint32_t open_duration_;

  sensor::Sensor *close_sensor_{nullptr};
  Trigger<> *close_trigger_{new Trigger<>()};
  float close_moving_current_threshold_;
  float close_obstacle_current_threshold_{FLT_MAX};
  float close_endstop_current_threshold_{FLT_MAX};
  number::Number *close_moving_current_threshold_number_{nullptr};
  number::Number *close_obstacle_current_threshold_number_{nullptr};
  number::Number *close_endstop_current_threshold_number_{nullptr};
  uint32_t close_duration_;

  uint32_t max_duration_{UINT32_MAX};
  uint32_t timeout_margin_{0};  // Extra time margin beyond calibrated duration
  bool malfunction_detection_{true};
  Trigger<> *malfunction_trigger_{new Trigger<>()};
  uint32_t start_sensing_delay_;
  uint32_t startup_delay_{0};  // Motor soft-starter delay (time before actual movement begins)
  float obstacle_rollback_;

  Trigger<> *prev_command_trigger_{nullptr};
  uint32_t last_recompute_time_{0};
  uint32_t start_dir_time_{0};
  uint32_t last_publish_time_{0};
  float target_position_{0};

  cover::CoverOperation last_operation_{cover::COVER_OPERATION_OPENING};

  // Advanced calibration features
  bool auto_calibration_on_boot_{false};
  bool save_calibration_{true};
  uint32_t endstop_detection_time_{1000};
  float calibration_endstop_threshold_{0.10f};  // Current threshold for calibration endstop detection
  number::Number *calibration_endstop_threshold_number_{nullptr};
  CalibrationState calibration_state_{CALIBRATION_IDLE};
  CalibrationData calibration_data_{0, 0, false};
  uint32_t calibration_start_time_{0};
  uint32_t endstop_detected_time_{0};
  uint32_t last_calibration_reading_{0};
  uint8_t endstop_confirmations_{0};  // Counter for consecutive endstop detections
  Trigger<> *calibration_complete_trigger_{new Trigger<>()};
  Trigger<> *calibration_failed_trigger_{new Trigger<>()};
  ESPPreferenceObject pref_;

  // Number entities for dynamic durations
  number::Number *open_duration_number_{nullptr};
  number::Number *close_duration_number_{nullptr};

  // Text sensor for calibration status
  text_sensor::TextSensor *calibration_status_sensor_{nullptr};
};

template<typename... Ts> class CalibrateAction : public Action<Ts...>, public Parented<AdvancedCurrentBasedCover> {
 public:
  void play(Ts... x) override { this->parent_->start_calibration(); }
};

template<typename... Ts> class ForceOpenAction : public Action<Ts...>, public Parented<AdvancedCurrentBasedCover> {
 public:
  void play(Ts... x) override { this->parent_->force_open(); }
};

template<typename... Ts> class ForceCloseAction : public Action<Ts...>, public Parented<AdvancedCurrentBasedCover> {
 public:
  void play(Ts... x) override { this->parent_->force_close(); }
};

}  // namespace advanced_current_based
}  // namespace esphome

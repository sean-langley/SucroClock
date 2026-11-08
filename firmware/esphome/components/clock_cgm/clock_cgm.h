#pragma once
#include "dexcom_client.h"
#include "esphome/core/component.h"
#include "esphome/components/aip33628/aip33628.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace esphome::clock_cgm {
class ClockCGM : public Component {
 public:
  void setup() override;
  void loop() override;
  void on_shutdown() override { stopped_ = true; }
  void dump_config() override;
  void set_panel(aip33628::Aip33628Panel *p) { panel_ = p; }
  void set_credentials(const std::string &u, const std::string &p) { username_ = u; password_ = p; }
  void set_source(const std::string &source);
  void set_region(const std::string &region);
  void set_display_mode(const std::string &mode);
  void retry();
  // Runtime settings are changed only on the ESPHome main loop.
  void update_credentials(const std::string &username, const std::string &password);
  void test_connection();
  const std::string &setup_username() const { return username_; }
  const std::string &setup_password() const { return password_; }
  const std::string &setup_status() const { return last_status_; }
  const std::string &test_status() const { return test_status_; }
  void set_glucose_sensor(sensor::Sensor *p) { glucose_sensor_ = p; }
  void set_age_sensor(sensor::Sensor *p) { age_sensor_ = p; }
  void set_status_sensor(text_sensor::TextSensor *p) { status_sensor_ = p; }
  void set_trend_sensor(text_sensor::TextSensor *p) { trend_sensor_ = p; }
  void set_measurement_time_sensor(text_sensor::TextSensor *p) { measurement_time_sensor_ = p; }
  void set_valid_sensor(binary_sensor::BinarySensor *p) { valid_sensor_ = p; }

 protected:
  struct Request { uint32_t generation; Region region; std::string username, password; };
  using Outcome = FetchOutcome;
  struct Result { uint32_t generation; Outcome outcome; Reading reading; };
  static void worker_entry_(void *arg);
  void worker_();
  void reset_();
  void publish_();
  void status_(const std::string &value);
  aip33628::Aip33628Panel *panel_{nullptr};
  sensor::Sensor *glucose_sensor_{nullptr}, *age_sensor_{nullptr};
  text_sensor::TextSensor *status_sensor_{nullptr}, *trend_sensor_{nullptr}, *measurement_time_sensor_{nullptr};
  binary_sensor::BinarySensor *valid_sensor_{nullptr};
  QueueHandle_t requests_{nullptr}, results_{nullptr};
  TaskHandle_t task_{nullptr};
  std::string selected_source_{"Home Assistant"};
  std::string username_, password_, last_status_, connection_status_{"Ready"};
  Reading reading_;
  Region region_{Region::US};
  bool dexcom_{false}, live_display_{false}, busy_{false}, auth_blocked_{false}, stopped_{false};
  uint32_t generation_{1}, next_poll_{0}, next_publish_{0}, accepted_at_{0};
  unsigned failures_{0};
  bool test_requested_{false};
  uint32_t test_generation_{0};
  std::string test_status_{"Not tested"};
};
}  // namespace esphome::clock_cgm

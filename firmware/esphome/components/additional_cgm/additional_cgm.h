#pragma once
#include "esphome/core/component.h"
#include "esphome/components/clock_cgm/provider_http.h"
#include "esphome/components/aip33628/aip33628.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/clock_cgm/sugarsense_client.h"
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
namespace esphome::additional_cgm {
using clock_cgm::ExtraConfig;using clock_cgm::Reading;using clock_cgm::FetchOutcome;
enum class Provider:uint8_t {LIBRE=0,NIGHTSCOUT=1,SUGARSENSE=2,NONE=3};
class AdditionalCGM:public Component {
 public:
  void setup() override;
  void loop() override;
  void on_shutdown() override {stopped_=true;}
  void dump_config() override;
  void set_panel(aip33628::Aip33628Panel *p){panel_=p;}
  void set_source(const std::string &source);
  void set_display_mode(const std::string &mode);
  void configure(Provider provider,const ExtraConfig &config);
  void test(Provider provider);
  void retry();
  void set_sugar_storage(std::function<bool(const std::string &)> save,const std::string &device){sugar_save_=std::move(save);sugar_device_=device;}
  void sugar_pair_start();
  void sugar_disconnect();
  bool sugar_pairing() const {return sugar_start_||sugar_poll_||sugar_inflight_;}
  bool sugar_paired() const {return clock_cgm::valid_sugar_token(configs_[2].secret);}
  const std::string &sugar_pair_status() const {return sugar_status_;}
  const std::string &sugar_code() const {return sugar_pair_.code;}
  const std::string &sugar_url() const {return sugar_pair_.url;}
  const std::string &test_status(Provider provider) const {return test_status_[size_t(provider)];}
  const std::string &choices() const {return choices_;}
  const std::string &status() const {return last_status_;}
  void set_glucose_sensor(sensor::Sensor *p){glucose_=p;}void set_age_sensor(sensor::Sensor *p){age_=p;}
  void set_status_sensor(text_sensor::TextSensor *p){status_=p;}void set_trend_sensor(text_sensor::TextSensor *p){trend_=p;}
  void set_measurement_time_sensor(text_sensor::TextSensor *p){stamp_=p;}void set_valid_sensor(binary_sensor::BinarySensor *p){valid_=p;}
 protected:
  enum class Operation:uint8_t {FETCH,PAIR_START,PAIR_POLL,UNPAIR};
  struct Request {uint32_t generation,config_generation;Provider provider;bool test;ExtraConfig config;Operation operation{Operation::FETCH};clock_cgm::SugarPair pair{};std::string device{};uint32_t pair_epoch{0};};
  struct Result {uint32_t generation,config_generation;Provider provider;bool test;FetchOutcome outcome;Reading reading;char choices[2048]{};Operation operation{Operation::FETCH};uint32_t pair_epoch{0};bool pair_pending{false};char code[7]{},poll_secret[64]{},pair_url[112]{},token[44]{};uint32_t expires_s{600},poll_s{5},pull_seconds{60};};
  static void worker_entry_(void *arg);void worker_();void reset_();void publish_();bool configured_(Provider p) const;
  aip33628::Aip33628Panel *panel_{nullptr};sensor::Sensor *glucose_{nullptr},*age_{nullptr};
  text_sensor::TextSensor *status_{nullptr},*trend_{nullptr},*stamp_{nullptr};binary_sensor::BinarySensor *valid_{nullptr};
  QueueHandle_t requests_{nullptr},results_{nullptr};TaskHandle_t task_{nullptr};
  ExtraConfig configs_[3];uint32_t config_generation_[3]{1,1,1};bool test_pending_[3]{false,false,false};
  std::string test_status_[3]{"Not tested","Not tested","Not tested"},choices_{"[]"},last_status_,connection_status_{"Ready"};
  std::function<bool(const std::string &)> sugar_save_;
  clock_cgm::SugarPair sugar_pair_;
  std::string sugar_device_,sugar_status_{"Not paired"},sugar_unpair_token_;
  uint32_t sugar_epoch_{1},sugar_expires_{0},sugar_next_poll_{0};
  bool sugar_start_{false},sugar_poll_{false},sugar_inflight_{false};
  void sugar_result_(const Result &result);
  void sugar_memory_failure_(Operation operation);
  Provider active_{Provider::NONE};Reading reading_;bool live_{false},busy_{false},blocked_{false},stopped_{false};
  uint32_t generation_{1},next_poll_{0},next_publish_{0},accepted_at_{0};unsigned failures_{0};
};
}

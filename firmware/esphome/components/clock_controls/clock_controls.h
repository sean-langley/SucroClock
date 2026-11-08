#pragma once
#include "commands.h"
#include "esphome/core/component.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"
#include "esphome/components/button/button.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/clock_cgm/clock_cgm.h"
#include "esphome/components/additional_cgm/additional_cgm.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <map>
namespace esphome::clock_controls {
class ClockControls:public Component {
 public:
  void setup() override;
  void loop() override;
  void set_panel(aip33628::Aip33628Panel *p){panel_=p;}
  void set_light(light::LightState *p){light_=p;}void set_connector(clock_cgm::ClockCGM *p){cgm_=p;}void set_additional(additional_cgm::AdditionalCGM *p){other_=p;}
  void add_switch(const std::string &k,switch_::Switch *p){switches_[k]=p;}
  void add_select(const std::string &k,select::Select *p){selects_[k]=p;}
  void add_number(const std::string &k,number::Number *p){numbers_[k]=p;}
  void add_button(const std::string &k,button::Button *p){buttons_[k]=p;}
  void add_binary_sensor(const std::string &k,binary_sensor::BinarySensor *p){binary_[k]=p;}
  void add_sensor(const std::string &k,sensor::Sensor *p){sensors_[k]=p;}
  void add_text_sensor(const std::string &k,text_sensor::TextSensor *p){texts_[k]=p;}
  int enqueue(const std::string &body);
  void snapshot(JsonObject root);
 protected:
  void apply_command_(const Command &c);
  void update_style_();
  bool persist_style_(const GlucoseStyle &config);
  aip33628::Aip33628Panel *panel_{nullptr};
  GlucoseStyle style_;StyleEngine style_engine_;StyleFrame style_frame_;
  uint32_t next_style_{0};
  light::LightState *light_{nullptr};clock_cgm::ClockCGM *cgm_{nullptr};additional_cgm::AdditionalCGM *other_{nullptr};
  std::map<std::string,switch_::Switch*> switches_;std::map<std::string,select::Select*> selects_;std::map<std::string,number::Number*> numbers_;
  std::map<std::string,button::Button*> buttons_;std::map<std::string,binary_sensor::BinarySensor*> binary_;std::map<std::string,sensor::Sensor*> sensors_;std::map<std::string,text_sensor::TextSensor*> texts_;
  QueueHandle_t commands_{nullptr};uint32_t revision_{0};std::string message_{"Ready"};
};
}

#pragma once
#include "settings.h"
#include "esphome/components/clock_controls/clock_controls.h"
#include "esphome/components/additional_cgm/additional_cgm.h"
#include "esphome/core/component.h"
#include "esphome/components/clock_cgm/clock_cgm.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <esp_https_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

namespace esphome::clock_setup {
class ClockSetup : public Component {
 public:
  void setup() override;
  void loop() override;
  void on_shutdown() override;
  void dump_config() override;
  float get_setup_priority() const override { return -50.0f; }
  void set_controls(clock_controls::ClockControls *p){controls_=p;}
  void set_additional(additional_cgm::AdditionalCGM *p){additional_=p;}
  void set_source_select(select::Select *p){source_select_=p;}
  void set_connector(clock_cgm::ClockCGM *p) { connector_ = p; }
  void set_region_select(select::Select *p) { region_select_ = p; }
  void set_tls(const std::string &cert, const std::string &key, const std::string &fingerprint) {
    certificate_ = cert; private_key_ = key; fingerprint_ = fingerprint;
  }
  void set_auth_password(const std::string &password) { auth_password_ = password; }
  void set_fingerprint_sensor(text_sensor::TextSensor *p) { fingerprint_sensor_ = p; }
  void set_url_sensor(text_sensor::TextSensor *p) { url_sensor_ = p; }
 protected:
  struct Command { char username[257]{}; char password[257]{}; char region[16]{}; char provider[16]{"dexcom"}; char patient[65]{}; char mode[16]{"Token"}; char client_version[16]{"4.16.0"}; bool forget{false}; bool test{true}; };
  static esp_err_t icon_handler_(httpd_req_t *request);
  static esp_err_t setup_handler_(httpd_req_t *request);
  static esp_err_t control_handler_(httpd_req_t *request);
  static esp_err_t page_handler_(httpd_req_t *request,bool setup);
  static esp_err_t root_handler_(httpd_req_t *request);
  static esp_err_t status_handler_(httpd_req_t *request);
  static esp_err_t save_handler_(httpd_req_t *request);
  bool authenticate_(httpd_req_t *request);
  static void headers_(httpd_req_t *request);
  static esp_err_t error_(httpd_req_t *request, const char *status, const char *message);
  void snapshot_();
  bool load_provider_(const char *space,ProviderSettings &record,bool libre);
  bool persist_provider_(const char *space,const ProviderSettings &record);
  bool load_();
  bool persist_sugar_(const std::string &token);
  bool persist_(const Credentials &credentials);
  clock_cgm::ClockCGM *connector_{nullptr};
  select::Select *region_select_{nullptr};
  text_sensor::TextSensor *fingerprint_sensor_{nullptr}, *url_sensor_{nullptr};
  httpd_handle_t server_{nullptr};
  QueueHandle_t commands_{nullptr};
  SemaphoreHandle_t mutex_{nullptr};
  clock_controls::ClockControls *controls_{nullptr};
  Credentials credentials_;
  ProviderSettings libre_,nightscout_;
  additional_cgm::AdditionalCGM *additional_{nullptr};
  select::Select *source_select_{nullptr};
  std::string certificate_, private_key_, fingerprint_, auth_password_, authorization_, csrf_, snapshot_json_;
  std::string save_status_{"No settings changes"};
  uint32_t revision_{0}, next_snapshot_{0};
  bool stopped_{false};
};
}  // namespace esphome::clock_setup

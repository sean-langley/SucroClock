#include "clock_cgm.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "provider_http.h"
#include <ctime>
#include <cmath>
#include <new>

namespace esphome::clock_cgm {
static const char *const TAG = "clock_cgm";
namespace {
class HttpTransport : public DexcomTransport {
 public:
  Reply post(Region region,const std::string &path,const std::string &payload) override {
    return https_request("POST",std::string(base_url(region))+path,{{"Content-Type","application/json"},{"Accept","application/json"}},payload,8192);
  }
};
}

void ClockCGM::setup() { init_provider_lock(); this->publish_(); }
void ClockCGM::dump_config() {
  ESP_LOGCONFIG(TAG, "Clock glucose connector: Dexcom Share, certificate verification enabled");
  ESP_LOGCONFIG(TAG, "  Poll every 60s; freshness limit 15min; worker requests do not block the main loop");
}
void ClockCGM::reset_() {
  ++generation_;
  test_requested_ = false;
  test_generation_ = 0;
  test_status_ = "Not tested";
  reading_ = {};
  auth_blocked_ = false;
  failures_ = 0;
  next_poll_ = millis();
  connection_status_ = "Ready";
  panel_->clear_provider_number();
  this->publish_();
}
void ClockCGM::set_source(const std::string &source) {
  selected_source_ = source;
  bool value = source == "Dexcom Share";
  if (dexcom_ == value) { this->publish_(); return; }
  dexcom_ = value;
  this->reset_();
}
void ClockCGM::set_region(const std::string &region) {
  Region value = region == "Outside US" ? Region::OUS : region == "Japan" ? Region::JP : Region::US;
  if (region_ == value) return;
  region_ = value;
  this->reset_();  // Invalidates both cached login and any in-flight result.
}
void ClockCGM::set_display_mode(const std::string &mode) {
  live_display_ = mode == "Display";
  panel_->clear_provider_number();
  this->publish_();
}
void ClockCGM::retry() { this->reset_(); }
void ClockCGM::update_credentials(const std::string &username, const std::string &password) {
  username_ = username;
  password_ = password;
  this->reset_();
}
void ClockCGM::test_connection() {
  this->reset_();
  if (username_.empty() || password_.empty()) { test_status_ = "Credentials required"; return; }
  test_requested_ = true;
  test_generation_ = generation_;
  test_status_ = "Waiting for network and clock sync";
}
void ClockCGM::status_(const std::string &value) {
  if (value == last_status_) return;
  last_status_ = value;
  status_sensor_->publish_state(value);
}
void ClockCGM::publish_() {
  int64_t now = std::time(nullptr);
  // Monotonic elapsed time prevents a backwards clock adjustment from keeping
  // a cached sample alive indefinitely. The measurement time is never replaced.
  bool usable = dexcom_ && fresh(reading_, now) && uint32_t(millis() - accepted_at_) <= 900000;
  if (!valid_sensor_->has_state() || valid_sensor_->state != usable) valid_sensor_->publish_state(usable);
  float glucose = usable ? reading_.mg_dl : NAN;
  if (!glucose_sensor_->has_state() || (std::isnan(glucose) != std::isnan(glucose_sensor_->state)) ||
      (!std::isnan(glucose) && glucose != glucose_sensor_->state)) glucose_sensor_->publish_state(glucose);
  float age = dexcom_ && reading_.present && now >= reading_.measured_at ? std::floor((now - reading_.measured_at) / 6.0) / 10.0f : NAN;
  if (!age_sensor_->has_state() || (std::isnan(age) != std::isnan(age_sensor_->state)) ||
      (!std::isnan(age) && age != age_sensor_->state)) age_sensor_->publish_state(age);
  const std::string trend = usable ? trend_name(reading_.trend) : "Unavailable";
  if (!trend_sensor_->has_state() || trend_sensor_->state != trend) trend_sensor_->publish_state(trend);
  if (dexcom_ && reading_.present) {
    std::time_t measured = reading_.measured_at;
    std::tm utc{};
    gmtime_r(&measured, &utc);
    char stamp[24];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
    if (!measurement_time_sensor_->has_state() || measurement_time_sensor_->state != stamp)
      measurement_time_sensor_->publish_state(stamp);
  } else {
    if (!measurement_time_sensor_->has_state() || measurement_time_sensor_->state != "Unavailable")
      measurement_time_sensor_->publish_state("Unavailable");
  }
  if (!dexcom_) this->status_(selected_source_ == "Home Assistant" ? "Home Assistant" : selected_source_ + " selected");
  else if (username_.empty() || password_.empty()) this->status_("Dexcom credentials required");
  else if (auth_blocked_) this->status_("Login rejected; check credentials/region, then Retry");
  else if (now < 1577836800) this->status_("Waiting for clock sync");
  else if (!network::is_connected()) this->status_("Wi-Fi disconnected");
  else if (reading_.present && !usable) this->status_("Reading stale or timestamp invalid");
  else if (usable && connection_status_ == "Ready") this->status_(live_display_ ? "Dexcom: fresh" : "Preview: fresh; clock display unchanged");
  else this->status_(connection_status_);
  if (usable && live_display_) panel_->show_provider_number(reading_.mg_dl, trend_glyph(reading_.trend));
  else panel_->clear_provider_number();
}

void ClockCGM::loop() {
  if (stopped_) return;
  Result result{};
  if (results_ && xQueueReceive(results_, &result, 0) == pdTRUE) {
    busy_ = false;
    if (result.generation == generation_) {
      if (test_generation_ == generation_) {
        switch (result.outcome) {
          case Outcome::OK: test_status_ = fresh(result.reading, std::time(nullptr)) ? "Connected; fresh reading received" : "Connected; reading is stale or timestamp invalid"; break;
          case Outcome::NO_DATA: test_status_ = "Connected; no readings available"; break;
          case Outcome::AUTH: test_status_ = "Login rejected; check account and region"; break;
          case Outcome::RATE_LIMIT: test_status_ = "Dexcom rate limited the request"; break;
          case Outcome::SESSION: test_status_ = "Session expired; retry the test"; break;
          case Outcome::INVALID_RESPONSE: test_status_ = "Dexcom returned an invalid response"; break;
          default: test_status_ = "Could not connect to Dexcom"; break;
        }
        test_generation_ = 0;
      }
      if (result.outcome == Outcome::OK) {
        // An older response must never move the reading backwards.
        if (!reading_.present || result.reading.measured_at > reading_.measured_at) {
          reading_ = result.reading;
          accepted_at_ = millis();
        }
        failures_ = 0;
        connection_status_ = "Ready";
        next_poll_ = millis() + 60000;
      } else {
        ++failures_;
        next_poll_ = millis() + retry_delay(failures_);
        switch (result.outcome) {
          case Outcome::AUTH: auth_blocked_ = true; connection_status_ = "Login rejected"; break;
          case Outcome::NO_DATA: reading_ = {}; connection_status_ = "No Dexcom readings"; break;
          case Outcome::SESSION: connection_status_ = "Session expired; reconnecting"; break;
          case Outcome::RATE_LIMIT: next_poll_ = millis() + 900000; connection_status_ = "Rate limited; waiting 15 minutes"; break;
          case Outcome::INVALID_RESPONSE: connection_status_ = "Invalid Dexcom response"; break;
          default: connection_status_ = "Connection failed; retry scheduled"; break;
        }
      }
    }
    this->publish_();
  }
  uint32_t now = millis();
  if ((int32_t)(now - next_publish_) >= 0) {
    next_publish_ = now + 1000;
    this->publish_();
  }
  if ((!dexcom_ && !test_requested_) || busy_ || auth_blocked_ || username_.empty() || password_.empty() ||
      !network::is_connected() || std::time(nullptr) < 1577836800 || (int32_t)(now - next_poll_) < 0) return;
  if (!task_) {
    if (!requests_) requests_ = xQueueCreate(1, sizeof(Request *));
    if (!results_) results_ = xQueueCreate(1, sizeof(Result));
    if (!requests_ || !results_ || xTaskCreate(worker_entry_, "dexcom_share", 12288, this, 1, &task_) != pdPASS) {
      connection_status_ = "Insufficient memory for Dexcom";
      next_poll_ = now + 60000;
      this->status_(connection_status_);
      return;
    }
  }
  auto *request = new (std::nothrow) Request{generation_, region_, username_, password_};
  if (!request) { next_poll_ = now + 60000; return; }
  if (xQueueSend(requests_, &request, 0) == pdTRUE) {
    busy_ = true;
    test_requested_ = false;
    if (test_generation_ == generation_) test_status_ = "Testing Dexcom connection";
  }
  else delete request;
}
void ClockCGM::worker_entry_(void *arg) { static_cast<ClockCGM *>(arg)->worker_(); }
void ClockCGM::worker_() {
  DexcomSession session;
  HttpTransport transport;
  for (;;) {
    Request *request = nullptr;
    xQueueReceive(requests_, &request, portMAX_DELAY);
    FetchResult fetched;
    { ProviderWorkGuard guard;
      if (guard.locked()) fetched = session.fetch(request->generation, request->region, request->username, request->password, transport);
    }
    Result result{request->generation, fetched.outcome, fetched.reading};
    delete request;
    // Only the main ESPHome loop publishes entities or touches the panel.
    xQueueSend(results_, &result, portMAX_DELAY);
  }
}
}  // namespace esphome::clock_cgm

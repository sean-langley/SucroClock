#include "clock_cgm.h"
#include <cassert>
#include <ctime>
#include <iostream>
using namespace esphome;
using namespace esphome::clock_cgm;
class Harness : public ClockCGM {
 public:
  aip33628::Aip33628Panel panel;
  sensor::Sensor glucose,age;
  text_sensor::TextSensor status,trend,stamp;
  binary_sensor::BinarySensor valid;
  Harness() {
    set_panel(&panel);set_glucose_sensor(&glucose);set_age_sensor(&age);
    set_status_sensor(&status);set_trend_sensor(&trend);set_measurement_time_sensor(&stamp);set_valid_sensor(&valid);
    set_credentials("u","p"); setup();
  }
  ~Harness() {
    if(requests_) {
      Request *r=nullptr;while(xQueueReceive(requests_,&r,0)==pdTRUE)delete r;
      delete requests_;
    }
    delete results_;
  }
  uint32_t generation() const{return generation_;}
  bool pending() const{return busy_;}
  size_t queued() const{return requests_ ? requests_->items.size() : 0;}
  uint32_t deadline() const{return next_poll_;}
  uint32_t accepted() const{return accepted_at_;}
  void finish(FetchOutcome outcome,Reading reading={},uint32_t gen=0) {
    assert(requests_ && results_);
    Request *request=nullptr;
    assert(xQueueReceive(requests_,&request,0)==pdTRUE);
    Result result{gen ? gen : request->generation,outcome,reading};delete request;
    assert(xQueueSend(results_,&result,0)==pdTRUE);loop();
  }
  void tick(uint32_t ms){fake_millis+=ms;loop();}
};
int main(){
  fake_millis=1000;
  Harness h;
  assert(h.status.state=="Home Assistant" && !h.pending() && !h.valid.state);
  h.loop();assert(h.queued()==0 && h.panel.updates==0);
  h.set_source("Dexcom Share");h.loop();assert(h.pending());
  Reading r{123,3,std::time(nullptr),true};
  h.finish(FetchOutcome::OK,r);assert(h.valid.state && h.glucose.state==123);
  assert(h.panel.updates==0 && h.status.state.find("Preview")==0);
  auto accepted=h.accepted();auto publishes=h.glucose.publications;
  h.tick(1000);assert(h.glucose.publications==publishes); // No sensor traffic for unchanged values.
  h.set_display_mode("Display");assert(h.panel.provider && h.panel.number==123);
  h.panel.manual_override=true;h.panel.provider=false;
  h.tick(1000);assert(!h.panel.provider); // Gateway delegates priority to panel's ownership guard.
  h.panel.manual_override=false;h.tick(1000);assert(h.panel.provider);
  h.tick(60000);assert(h.pending());h.finish(FetchOutcome::OK,r);
  assert(h.accepted()==accepted); // Repeated server sample never refreshes its age.
  h.tick(901000);assert(!h.valid.state && std::isnan(h.glucose.state) && !h.panel.provider);
  auto old_generation=h.generation();
  assert(h.pending());h.set_region("Outside US");h.finish(FetchOutcome::OK,r,old_generation);
  assert(!h.valid.state && h.generation()!=old_generation); // Late response from old region ignored.
  assert(h.pending());h.finish(FetchOutcome::AUTH);assert(!h.pending());
  h.tick(3600000);assert(!h.pending()); // Bad password cannot trigger repeated logins.
  h.retry();h.loop();assert(h.pending());
  h.finish(FetchOutcome::NETWORK);assert(!h.pending());
  auto deadline=h.deadline();h.tick(59999);assert(!h.pending());
  h.tick(1);assert(h.pending() && fake_millis==deadline);
  h.finish(FetchOutcome::RATE_LIMIT);h.tick(899999);assert(!h.pending());
  h.tick(1);assert(h.pending());
  h.finish(FetchOutcome::NO_DATA);assert(!h.valid.state && std::isnan(h.glucose.state));
  h.set_source("Home Assistant");h.tick(3600000);
  assert(!h.pending() && !h.panel.provider && h.status.state=="Home Assistant");
  h.set_credentials("","");h.set_source("Dexcom Share");h.loop();
  assert(!h.pending() && h.status.state=="Dexcom credentials required");
  h.update_credentials("updated-user", "updated-password");
  h.set_source("Home Assistant");h.test_connection();h.loop();
  assert(h.pending() && h.test_status()=="Testing Dexcom connection");
  h.finish(FetchOutcome::OK,Reading{130,4,std::time(nullptr),true});
  assert(h.test_status()=="Connected; fresh reading received");
  assert(!h.panel.provider && !h.valid.state && h.status.state=="Home Assistant");
  h.tick(3600000);assert(!h.pending()); // Save & Test is one-shot in HA mode.
  h.test_connection();h.loop();assert(h.pending());
  h.update_credentials("new-user", "new-password");
  h.finish(FetchOutcome::OK,Reading{140,4,std::time(nullptr),true});
  assert(h.test_status()=="Not tested" && !h.valid.state); // Old credential result cannot leak through.
  h.update_credentials("", "");h.test_connection();h.loop();
  assert(!h.pending() && h.test_status()=="Credentials required");
  std::cout<<"Source isolation, preview, freshness, stale results, retry/backoff and login-stop tests passed.\n";
}

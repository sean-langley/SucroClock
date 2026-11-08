#include "esphome/components/additional_cgm/additional_cgm.h"
#include <cassert>
#include <ctime>
#include <cstring>
#include <iostream>
using namespace esphome;using namespace esphome::additional_cgm;using namespace esphome::clock_cgm;
class Harness:public AdditionalCGM {
 public:
  aip33628::Aip33628Panel panel;sensor::Sensor glucose,age;text_sensor::TextSensor status,trend,stamp;binary_sensor::BinarySensor valid;
  Harness(){set_panel(&panel);set_glucose_sensor(&glucose);set_age_sensor(&age);set_status_sensor(&status);set_trend_sensor(&trend);set_measurement_time_sensor(&stamp);set_valid_sensor(&valid);setup();}
  ~Harness(){if(requests_){Request *r=nullptr;while(xQueueReceive(requests_,&r,0)==pdTRUE)delete r;delete requests_;}delete results_;}
  void pair_finish(FetchOutcome outcome,const std::string &token="",bool pending=false){
   Request *request=nullptr;assert(xQueueReceive(requests_,&request,0)==pdTRUE);
   Result result{request->generation,request->config_generation,request->provider,request->test,outcome,{}, {}};
   result.operation=request->operation;result.pair_epoch=request->pair_epoch;result.pair_pending=pending;std::strcpy(result.code,"ABC123");std::strcpy(result.poll_secret,"private");std::strcpy(result.pair_url,"https://app.sugarsense.io/display?c=ABC123");std::strcpy(result.token,token.c_str());result.expires_s=30;result.poll_s=2;delete request;
   assert(xQueueSend(results_,&result,0)==pdTRUE);loop();
  }
  bool queued_unpair() const {Request *request=nullptr;if(!requests_||requests_->items.empty())return false;std::memcpy(&request,requests_->items.front().data(),sizeof(request));return request->operation==Operation::UNPAIR;}
  bool pending()const{return busy_;}uint32_t epoch()const{return generation_;}
  void tick(uint32_t ms){fake_millis+=ms;loop();}
  void finish(FetchOutcome outcome,Reading reading={},const char *choices="[]"){
   Request *request=nullptr;assert(xQueueReceive(requests_,&request,0)==pdTRUE);
   Result result{request->generation,request->config_generation,request->provider,request->test,outcome,reading,{}};std::strcpy(result.choices,choices);delete request;
   assert(xQueueSend(results_,&result,0)==pdTRUE);loop();
  }
};
int main(){
 fake_millis=1000;Harness h;ExtraConfig libre;libre.address="email";libre.secret="password";
 ExtraConfig ns;ns.address="https://example.test";ns.mode="Public";
 h.panel.show_provider_number(111,"-",1);h.configure(Provider::LIBRE,libre);h.configure(Provider::NIGHTSCOUT,ns);h.loop();
 assert(h.panel.provider&&h.panel.owner==1&&h.panel.number==111&&!h.pending());
 h.test(Provider::LIBRE);h.loop();assert(h.pending());h.finish(FetchOutcome::OK,{123,3,std::time(nullptr),true});
 assert(h.panel.owner==1&&h.panel.number==111&&!h.valid.state);h.tick(3600000);assert(!h.pending()); // Test cannot replace Dexcom or start recurring fetches.
 h.panel.clear_provider_number(1);h.set_source("LibreLinkUp");h.loop();h.finish(FetchOutcome::OK,{130,4,std::time(nullptr),true});
 assert(h.valid.state&&!h.panel.provider); // Preview remains the default.
 h.set_display_mode("Display");assert(h.panel.provider&&h.panel.owner==2&&h.panel.number==130);
 h.panel.clear_provider_number(1);h.tick(1000);assert(h.panel.owner==2); // Inactive Dexcom cannot clear the active other provider.
 h.test(Provider::NIGHTSCOUT);h.loop();assert(h.pending());h.finish(FetchOutcome::AUTH);
 assert(h.valid.state&&h.panel.number==130&&h.panel.owner==2); // Testing another provider cannot corrupt the active reading.
 h.tick(60000);assert(h.pending());auto epoch=h.epoch();h.set_source("Nightscout");assert(h.epoch()!=epoch);h.finish(FetchOutcome::OK,{222,4,std::time(nullptr),true});
 assert(!h.valid.state&&h.pending());h.finish(FetchOutcome::OK,{140,4,std::time(nullptr),true});assert(h.valid.state&&h.panel.number==140);
 h.tick(60000);assert(h.pending());h.finish(FetchOutcome::AUTH);h.tick(3600000);assert(!h.pending()&&!h.valid.state);
 h.retry();h.loop();assert(h.pending());h.finish(FetchOutcome::NO_DATA);assert(!h.valid.state);
 h.set_source("Dexcom Share");h.panel.show_provider_number(150,"-",1);h.tick(60000);assert(h.panel.owner==1&&h.panel.number==150&&!h.pending());
 h.configure(Provider::LIBRE,libre);h.test(Provider::LIBRE);h.loop();assert(h.pending());h.configure(Provider::LIBRE,ExtraConfig{});h.finish(FetchOutcome::OK,{199,4,std::time(nullptr),true});
 assert(h.panel.number==150&&h.test_status(Provider::LIBRE)=="Not tested");
 {
 Harness sugar;std::string saved;bool writes=true;const std::string token(43,'a');sugar.set_sugar_storage([&](const std::string &value){if(!writes)return false;saved=value;return true;},"disp-test");
 sugar.panel.show_provider_number(120,"-",1);sugar.sugar_pair_start();sugar.loop();assert(sugar.pending());sugar.pair_finish(FetchOutcome::OK);assert(sugar.sugar_code()=="ABC123"&&saved.empty());
 sugar.tick(2000);assert(sugar.pending());sugar.pair_finish(FetchOutcome::OK,"",true);assert(!sugar.sugar_paired());
 sugar.tick(2000);sugar.pair_finish(FetchOutcome::OK,token);assert(sugar.sugar_paired()&&saved==token&&sugar.sugar_code().empty());assert(sugar.panel.owner==1&&sugar.pending());
 sugar.finish(FetchOutcome::OK,{123,4,std::time(nullptr),true});assert(sugar.panel.owner==1&&!sugar.valid.state); // Pairing/test never changes source.
 sugar.set_source("SugarSense");sugar.set_display_mode("Display");sugar.panel.clear_provider_number(1);sugar.loop();sugar.finish(FetchOutcome::OK,{125,4,std::time(nullptr),true});assert(sugar.valid.state&&sugar.panel.owner==2);
 sugar.tick(901000);assert(!sugar.valid.state&&!sugar.panel.provider);sugar.finish(FetchOutcome::AUTH);sugar.tick(3600000);assert(!sugar.pending());
 writes=false;sugar.sugar_disconnect();assert(sugar.sugar_paired()&&saved==token);writes=true;sugar.sugar_disconnect();assert(!sugar.sugar_paired()&&saved.empty());sugar.loop();assert(sugar.queued_unpair());sugar.pair_finish(FetchOutcome::NETWORK);assert(sugar.sugar_pair_status().find("revoke")!=std::string::npos);
 sugar.sugar_pair_start();sugar.loop();sugar.pair_finish(FetchOutcome::OK);sugar.tick(31000);assert(sugar.sugar_code().empty()&&!sugar.pending());
 sugar.sugar_pair_start();sugar.loop();sugar.pair_finish(FetchOutcome::OK);sugar.tick(2000);assert(sugar.pending());sugar.sugar_disconnect();sugar.pair_finish(FetchOutcome::OK,token);assert(saved.empty()&&!sugar.sugar_paired()&&sugar.queued_unpair());sugar.pair_finish(FetchOutcome::OK);
 sugar.sugar_pair_start();sugar.loop();sugar.pair_finish(FetchOutcome::OK);sugar.tick(2000);writes=false;sugar.pair_finish(FetchOutcome::OK,token);assert(!sugar.sugar_paired()&&saved.empty()&&sugar.queued_unpair());sugar.pair_finish(FetchOutcome::OK);
 }
 {Harness memory;fake_task_success=false;memory.sugar_pair_start();memory.loop();assert(!memory.sugar_pairing()&&memory.sugar_pair_status().find("Insufficient memory")!=std::string::npos);memory.tick(1000);assert(!memory.pending());fake_task_success=true;}
 std::cout<<"Provider test/display isolation, source switching, preview, stale results and auth-stop tests passed.\n";
}

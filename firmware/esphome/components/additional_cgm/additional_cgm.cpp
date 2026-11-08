#include "additional_cgm.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include <ctime>
#include <cmath>
#include <cstring>
#include <new>
namespace esphome::additional_cgm {
static const char *const TAG="additional_cgm";
namespace {
const char *name(Provider p){return p==Provider::LIBRE?"LibreLinkUp":p==Provider::SUGARSENSE?"SugarSense":"Nightscout";}
const char *description(FetchOutcome outcome){
 switch(outcome){
 case FetchOutcome::OK:return "Connected; reading received";
 case FetchOutcome::NO_DATA:return "Connected; no readings available";
 case FetchOutcome::AUTH:return "Login rejected; check provider settings";
 case FetchOutcome::SESSION:return "Session expired; retry scheduled";
 case FetchOutcome::RATE_LIMIT:return "Rate limited; waiting 15 minutes";
 case FetchOutcome::ACTION_REQUIRED:return "Open LibreLinkUp app to finish account setup/accept terms or update client version";
 case FetchOutcome::SELECTION_REQUIRED:return "Choose a linked person, then Save & Test";
 case FetchOutcome::INVALID_RESPONSE:return "Invalid provider response or configuration";
 default:return "Connection failed; retry scheduled";
 }
}
void publish_number(sensor::Sensor *sensor,float value){if(!sensor->has_state()||(std::isnan(value)!=std::isnan(sensor->state))||(!std::isnan(value)&&value!=sensor->state))sensor->publish_state(value);}
void publish_text(text_sensor::TextSensor *sensor,const std::string &value){if(!sensor->has_state()||sensor->state!=value)sensor->publish_state(value);}
}
void AdditionalCGM::setup(){clock_cgm::init_provider_lock();publish_();}
void AdditionalCGM::dump_config(){ESP_LOGCONFIG(TAG,"LibreLinkUp, Nightscout and SugarSense connectors; TLS verified; no automatic provider fallback");}
bool AdditionalCGM::configured_(Provider p) const {
 if(p==Provider::NONE)return false;const auto &c=configs_[size_t(p)];
 if(p==Provider::SUGARSENSE)return clock_cgm::valid_sugar_token(c.secret);
 return !c.address.empty()&&((p==Provider::NIGHTSCOUT&&c.mode=="Public")||!c.secret.empty());
}
void AdditionalCGM::reset_(){++generation_;reading_={};failures_=0;blocked_=false;next_poll_=millis();connection_status_="Ready";panel_->clear_provider_number(2);publish_();}
void AdditionalCGM::set_source(const std::string &source){Provider next=source=="LibreLinkUp"?Provider::LIBRE:source=="Nightscout"?Provider::NIGHTSCOUT:source=="SugarSense"?Provider::SUGARSENSE:Provider::NONE;if(next!=active_){active_=next;reset_();}}
void AdditionalCGM::set_display_mode(const std::string &mode){live_=mode=="Display";panel_->clear_provider_number(2);publish_();}
void AdditionalCGM::configure(Provider p,const ExtraConfig &config){if(p==Provider::NONE)return;size_t i=size_t(p);configs_[i]=config;++config_generation_[i];test_pending_[i]=false;test_status_[i]="Not tested";if(p==Provider::LIBRE)choices_="[]";if(p==Provider::SUGARSENSE)sugar_status_=sugar_paired()?"Paired":"Not paired";if(active_==p)reset_();}
void AdditionalCGM::test(Provider p){if(p==Provider::NONE)return;size_t i=size_t(p);if(!configured_(p)){test_status_[i]="Provider settings required";return;}test_pending_[i]=true;test_status_[i]="Waiting for connection test";if(active_==p)blocked_=false;}
void AdditionalCGM::retry(){if(active_!=Provider::NONE){++config_generation_[size_t(active_)];reset_();}}
void AdditionalCGM::publish_(){
 int64_t now=std::time(nullptr);bool usable=active_!=Provider::NONE&&clock_cgm::fresh(reading_,now)&&uint32_t(millis()-accepted_at_)<=900000;
 if(!valid_->has_state()||valid_->state!=usable)valid_->publish_state(usable);
 publish_number(glucose_,usable?reading_.mg_dl:NAN);
 publish_number(age_,active_!=Provider::NONE&&reading_.present&&now>=reading_.measured_at?std::floor((now-reading_.measured_at)/6.0)/10.0f:NAN);
 publish_text(trend_,usable?clock_cgm::trend_name(reading_.trend):"Unavailable");
 std::string stamp="Unavailable";if(active_!=Provider::NONE&&reading_.present){std::time_t measured=reading_.measured_at;std::tm utc{};gmtime_r(&measured,&utc);char buffer[24];std::strftime(buffer,sizeof(buffer),"%Y-%m-%dT%H:%M:%SZ",&utc);stamp=buffer;}publish_text(stamp_,stamp);
 std::string status;
 if(active_==Provider::NONE)status="Inactive; current source is Home Assistant or Dexcom";
 else if(!configured_(active_))status=std::string(name(active_))+" settings required";
 else if(blocked_)status=connection_status_;
 else if(now<1577836800)status="Waiting for clock sync";
 else if(!network::is_connected())status="Wi-Fi disconnected";
 else if(reading_.present&&!usable)status="Reading stale or timestamp invalid";
 else if(usable&&connection_status_=="Ready")status=std::string(name(active_))+(live_?": fresh":" preview: fresh; clock display unchanged");
 else status=connection_status_;
 last_status_=status;publish_text(status_,status);
 if(usable&&live_)panel_->show_provider_number(reading_.mg_dl,clock_cgm::trend_glyph(reading_.trend),2);else panel_->clear_provider_number(2);
}
void AdditionalCGM::loop(){
 if(stopped_)return;Result result{};
 if(results_&&xQueueReceive(results_,&result,0)==pdTRUE){
  busy_=false;size_t i=size_t(result.provider);
  if(result.operation!=Operation::FETCH){sugar_result_(result);}
  else if(result.config_generation==config_generation_[i]){
   if(result.provider==Provider::LIBRE)choices_=result.choices;
   if(result.test){test_status_[i]=description(result.outcome);
    if(result.outcome==FetchOutcome::NETWORK)test_status_[i]="Could not connect to provider";
    if(result.outcome==FetchOutcome::SESSION)test_status_[i]="Session expired; retry the test";
    if(result.outcome==FetchOutcome::RATE_LIMIT)test_status_[i]="Provider rate limited the test; try later";
    if(result.outcome==FetchOutcome::OK)test_status_[i]=clock_cgm::fresh(result.reading,std::time(nullptr))?"Connected; fresh reading received":"Connected; reading stale or timestamp invalid";}
   if(result.generation==generation_&&active_==result.provider){
    if(result.outcome==FetchOutcome::OK){if(!reading_.present||result.reading.measured_at>reading_.measured_at){reading_=result.reading;accepted_at_=millis();}failures_=0;blocked_=false;next_poll_=millis()+result.pull_seconds*1000;connection_status_="Ready";}
    else {++failures_;next_poll_=millis()+clock_cgm::retry_delay(failures_);connection_status_=result.provider==Provider::SUGARSENSE&&result.outcome==FetchOutcome::AUTH?"SugarSense session rejected; disconnect and pair again":description(result.outcome);
     if(result.outcome==FetchOutcome::NO_DATA)reading_={};
     if(result.outcome==FetchOutcome::AUTH||result.outcome==FetchOutcome::ACTION_REQUIRED||result.outcome==FetchOutcome::SELECTION_REQUIRED){blocked_=true;reading_={};}
     if(result.outcome==FetchOutcome::RATE_LIMIT)next_poll_=millis()+900000;
    }
   }
  }
  publish_();
 }
 uint32_t now=millis();if((int32_t)(now-next_publish_)>=0){next_publish_=now+1000;publish_();}
 if(busy_||!network::is_connected()||std::time(nullptr)<1577836800)return;
 Provider target=Provider::NONE;bool is_test=false;
 for(size_t i=0;i<3;++i)if(test_pending_[i]){target=Provider(i);is_test=true;break;}
 if(target==Provider::NONE&&active_!=Provider::NONE&&!blocked_&&(int32_t)(now-next_poll_)>=0)target=active_;
 Operation operation=Operation::FETCH;
 if(sugar_poll_&&(int32_t)(now-sugar_expires_)>=0){sugar_poll_=false;sugar_pair_={};sugar_status_="Pairing code expired; start again";}
 if(!sugar_unpair_token_.empty())operation=Operation::UNPAIR;
 else if(sugar_start_)operation=Operation::PAIR_START;
 else if(sugar_poll_&&(int32_t)(now-sugar_next_poll_)>=0)operation=Operation::PAIR_POLL;
 if(operation!=Operation::FETCH){target=Provider::SUGARSENSE;is_test=false;}
 if(target==Provider::NONE||(operation==Operation::FETCH&&!configured_(target)))return;
 if(!task_){if(!requests_)requests_=xQueueCreate(1,sizeof(Request*));if(!results_)results_=xQueueCreate(1,sizeof(Result));
  if(!requests_||!results_||xTaskCreate(worker_entry_,"other_cgm",12288,this,1,&task_)!=pdPASS){next_poll_=now+60000;connection_status_="Insufficient memory for provider";sugar_memory_failure_(operation);if(is_test){test_pending_[size_t(target)]=false;test_status_[size_t(target)]=connection_status_;}return;}}
 size_t i=size_t(target);auto *request=new(std::nothrow)Request{generation_,config_generation_[i],target,is_test,configs_[i]};
 if(request){request->operation=operation;request->pair_epoch=sugar_epoch_;request->pair=sugar_pair_;request->device=sugar_device_;if(operation==Operation::UNPAIR)request->config.secret=sugar_unpair_token_;}
 if(!request){next_poll_=now+60000;sugar_memory_failure_(operation);return;}
 if(xQueueSend(requests_,&request,0)==pdTRUE){busy_=true;
  if(operation==Operation::PAIR_START){sugar_start_=false;sugar_inflight_=true;}
  if(operation==Operation::PAIR_POLL){sugar_next_poll_=now+sugar_pair_.poll_s*1000;sugar_inflight_=true;}
  if(operation==Operation::UNPAIR)sugar_unpair_token_.clear();
  if(is_test){test_pending_[i]=false;test_status_[i]="Testing connection";}}else delete request;
}
void AdditionalCGM::worker_entry_(void *arg){static_cast<AdditionalCGM*>(arg)->worker_();}
void AdditionalCGM::sugar_memory_failure_(Operation operation){
 if(operation==Operation::FETCH)return;
 sugar_start_=false;sugar_poll_=false;sugar_inflight_=false;sugar_pair_={};sugar_unpair_token_.clear();
 sugar_status_=operation==Operation::UNPAIR?"Disconnected locally; revoke the display at app.sugarsense.io/display":"Insufficient memory to pair; try again later";
}
void AdditionalCGM::sugar_pair_start(){
 if(sugar_paired()){sugar_status_="Already paired; disconnect before pairing another account";return;}
 if(sugar_pairing()){return;}
 ++sugar_epoch_;sugar_pair_={};sugar_start_=true;sugar_status_="Waiting for network and clock sync";
}
void AdditionalCGM::sugar_disconnect(){
 if(!sugar_save_||!sugar_save_("")){sugar_status_="Could not remove saved session; previous settings retained";return;}
 auto token=configs_[2].secret;++sugar_epoch_;sugar_start_=false;sugar_poll_=false;sugar_inflight_=false;sugar_pair_={};
 configure(Provider::SUGARSENSE,ExtraConfig{});sugar_unpair_token_=token;
 sugar_status_=token.empty()?"Pairing cancelled":"Disconnected locally; revoking server session";
}
void AdditionalCGM::sugar_result_(const Result &r){
 if(r.pair_epoch!=sugar_epoch_){if(clock_cgm::valid_sugar_token(r.token))sugar_unpair_token_=r.token;return;}
 sugar_inflight_=false;
 if(r.operation==Operation::UNPAIR){sugar_status_=r.outcome==FetchOutcome::OK?"Disconnected":"Disconnected locally; revoke the display at app.sugarsense.io/display if needed";return;}

 if(r.operation==Operation::PAIR_START){
  if(r.outcome!=FetchOutcome::OK){sugar_status_="Could not start pairing; try again later";return;}
  sugar_pair_.code=r.code;sugar_pair_.secret=r.poll_secret;sugar_pair_.url=r.pair_url;sugar_pair_.expires_s=r.expires_s;sugar_pair_.poll_s=r.poll_s;
  sugar_expires_=millis()+r.expires_s*1000;sugar_next_poll_=millis()+r.poll_s*1000;sugar_poll_=true;sugar_status_="Approve this code in your SugarSense account";return;
 }
 if(!sugar_poll_||(int32_t)(millis()-sugar_expires_)>=0){if(clock_cgm::valid_sugar_token(r.token))sugar_unpair_token_=r.token;sugar_poll_=false;sugar_pair_={};sugar_status_="Pairing code expired; start again";return;}
 if(r.outcome==FetchOutcome::OK&&r.pair_pending){sugar_status_="Waiting for approval in SugarSense";return;}
 if(r.outcome==FetchOutcome::OK&&clock_cgm::valid_sugar_token(r.token)){
  sugar_poll_=false;sugar_pair_={};
  if(!sugar_save_||!sugar_save_(r.token)){sugar_unpair_token_=r.token;sugar_status_="Could not save session; please pair again";return;}
  ExtraConfig config;config.secret=r.token;configure(Provider::SUGARSENSE,config);sugar_status_="Paired. Select SugarSense as the glucose source when ready";test(Provider::SUGARSENSE);return;
 }
 if(r.outcome==FetchOutcome::NETWORK||r.outcome==FetchOutcome::RATE_LIMIT){sugar_next_poll_=millis()+(r.outcome==FetchOutcome::RATE_LIMIT?60000:15000);sugar_status_="Pairing connection interrupted; retrying while the code is valid";return;}
 sugar_poll_=false;sugar_pair_={};sugar_status_="Pairing expired or rejected; start again";
}
void AdditionalCGM::worker_(){
 clock_cgm::LibreSession libre;clock_cgm::SecureProviderTransport transport;
 for(;;){
  Request *request=nullptr;xQueueReceive(requests_,&request,portMAX_DELAY);clock_cgm::ExtraResult fetched;clock_cgm::SugarPairResult pair;
  {clock_cgm::ProviderWorkGuard guard;if(guard.locked()){
   switch(request->operation){
    case Operation::PAIR_START:pair=clock_cgm::sugar_pair_start(transport);fetched.outcome=pair.outcome;break;
    case Operation::PAIR_POLL:pair=clock_cgm::sugar_pair_poll(request->pair,request->device,transport);fetched.outcome=pair.outcome;break;
    case Operation::UNPAIR:fetched.outcome=clock_cgm::sugar_unpair(request->config.secret,transport);break;
    default:fetched=request->provider==Provider::LIBRE?libre.fetch(request->config_generation,request->config,transport):request->provider==Provider::SUGARSENSE?clock_cgm::fetch_sugarsense(request->config,transport):clock_cgm::fetch_nightscout(request->config,transport);break;
   }
  }}
  Result result{request->generation,request->config_generation,request->provider,request->test,fetched.outcome,fetched.reading,{}};
  result.pull_seconds=fetched.pull_seconds;result.operation=request->operation;result.pair_epoch=request->pair_epoch;result.pair_pending=pair.pending;
  auto copy=[](auto &dest,const std::string &value){if(value.size()<sizeof(dest))std::memcpy(dest,value.c_str(),value.size()+1);};
  copy(result.choices,fetched.choices);copy(result.code,pair.pair.code);copy(result.poll_secret,pair.pair.secret);copy(result.pair_url,pair.pair.url);copy(result.token,pair.token);result.expires_s=pair.pair.expires_s;result.poll_s=pair.pair.poll_s;
  delete request;xQueueSend(results_,&result,portMAX_DELAY);
 }
}
}

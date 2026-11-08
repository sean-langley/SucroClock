#include "clock_setup.h"
#include "page.h"
#include "dashboard.h"
#include "brand_icon.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <ArduinoJson.h>
#include <esp_random.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>
#include <nvs.h>
#include <algorithm>
#include <cstdio>

namespace esphome::clock_setup {
static const char *const TAG = "clock_setup";
void ClockSetup::headers_(httpd_req_t *request) {
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
  httpd_resp_set_hdr(request, "X-Frame-Options", "DENY");
  httpd_resp_set_hdr(request, "Referrer-Policy", "no-referrer");
}
esp_err_t ClockSetup::error_(httpd_req_t *request, const char *status, const char *message) {
  headers_(request);
  httpd_resp_set_status(request, status);
  httpd_resp_set_type(request, "application/json");
  JsonDocument doc;doc["error"] = message;
  std::string body;serializeJson(doc, body);
  return httpd_resp_send(request, body.c_str(), body.size());
}
bool ClockSetup::authenticate_(httpd_req_t *request) {
  size_t length = httpd_req_get_hdr_value_len(request, "Authorization");
  std::string value(length <= 1024 ? length + 1 : 1, '\0');
  bool authenticated = length > 0 && length <= 1024 &&
    httpd_req_get_hdr_value_str(request, "Authorization", value.data(), value.size()) == ESP_OK;
  if (authenticated) { value.resize(length); authenticated = constant_equal(value, authorization_); }
  if (!authenticated) {
    httpd_resp_set_hdr(request, "WWW-Authenticate", "Basic realm=\"SucroClock Secure Setup\"");
    error_(request, "401 Unauthorized", "Login required");
  }
  return authenticated;
}
bool ClockSetup::load_() {
  nvs_handle_t handle;
  if (nvs_open("clock_dexcom", NVS_READONLY, &handle) != ESP_OK) return false;
  Credentials record;size_t size = sizeof(record);
  esp_err_t error = nvs_get_blob(handle, "credentials", &record, &size);
  nvs_close(handle);
  if (error != ESP_OK || size != sizeof(record) || !valid_record(record)) return false;
  credentials_ = record;
  return true;
}
bool ClockSetup::persist_(const Credentials &credentials) {
  nvs_handle_t handle;
  if (nvs_open("clock_dexcom", NVS_READWRITE, &handle) != ESP_OK) return false;
  esp_err_t error = nvs_set_blob(handle, "credentials", &credentials, sizeof(credentials));
  if (error == ESP_OK) error = nvs_commit(handle);
  nvs_close(handle);
  return error == ESP_OK;
}
bool ClockSetup::load_provider_(const char *space,ProviderSettings &record,bool libre) {
  nvs_handle_t handle;if(nvs_open(space,NVS_READONLY,&handle)!=ESP_OK)return false;
  ProviderSettings loaded;size_t size=sizeof(loaded);auto error=nvs_get_blob(handle,"settings",&loaded,&size);nvs_close(handle);
  if(error!=ESP_OK||size!=sizeof(loaded)||!valid_provider_record(loaded,libre))return false;record=loaded;return true;
}
bool ClockSetup::persist_provider_(const char *space,const ProviderSettings &record) {
  nvs_handle_t handle;if(nvs_open(space,NVS_READWRITE,&handle)!=ESP_OK)return false;
  auto error=nvs_set_blob(handle,"settings",&record,sizeof(record));if(error==ESP_OK)error=nvs_commit(handle);nvs_close(handle);return error==ESP_OK;
}
bool ClockSetup::persist_sugar_(const std::string &token){
 if(!token.empty()&&!clock_cgm::valid_sugar_token(token))return false;
 SugarSession record;std::copy(token.begin(),token.end(),record.token);
 nvs_handle_t handle;if(nvs_open("clock_sugar",NVS_READWRITE,&handle)!=ESP_OK)return false;
 auto error=nvs_set_blob(handle,"session",&record,sizeof(record));if(error==ESP_OK)error=nvs_commit(handle);nvs_close(handle);return error==ESP_OK;
}
void ClockSetup::setup() {
  if (auth_password_.empty()) { ESP_LOGE(TAG, "HTTPS setup requires a login password"); mark_failed(); return; }
  commands_ = xQueueCreate(1, sizeof(Command));
  mutex_ = xSemaphoreCreateMutex();
  if (!commands_ || !mutex_) { mark_failed(); return; }
  if (load_()) {
    connector_->update_credentials(credentials_.username, credentials_.password);
    save_status_ = credentials_.username[0] ? "Saved credentials loaded" : "Dexcom credentials cleared";
  } else {
    auto user = connector_->setup_username();auto password = connector_->setup_password();
    if (user.size() <= MAX_CREDENTIAL && password.size() <= MAX_CREDENTIAL) {
      std::copy(user.begin(), user.end(), credentials_.username);
      std::copy(password.begin(), password.end(), credentials_.password);
    }
  }
  load_provider_("clock_libre",libre_,true);load_provider_("clock_ns",nightscout_,false);
  additional_->configure(additional_cgm::Provider::LIBRE,extra_config(libre_));
  additional_->configure(additional_cgm::Provider::NIGHTSCOUT,extra_config(nightscout_));
  uint8_t mac[6]{};esp_read_mac(mac,ESP_MAC_WIFI_STA);char device[24];
  std::snprintf(device,sizeof(device),"disp-%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  additional_->set_sugar_storage([this](const std::string &token){return persist_sugar_(token);},device);
  nvs_handle_t sugar_handle;SugarSession sugar;
  if(nvs_open("clock_sugar",NVS_READONLY,&sugar_handle)==ESP_OK){size_t size=sizeof(sugar);auto error=nvs_get_blob(sugar_handle,"session",&sugar,&size);nvs_close(sugar_handle);if(error==ESP_OK&&size==sizeof(sugar)&&valid_sugar_session(sugar)){clock_cgm::ExtraConfig config;config.secret=sugar.token;additional_->configure(additional_cgm::Provider::SUGARSENSE,config);}}
  uint8_t random[32];esp_fill_random(random, sizeof(random));
  const char *hex = "0123456789abcdef";
  for (uint8_t byte : random) {csrf_ += hex[byte >> 4];csrf_ += hex[byte & 15];}
  std::string login = "admin:" + auth_password_;
  unsigned char encoded[1024];size_t written = 0;
  if (mbedtls_base64_encode(encoded, sizeof(encoded), &written, reinterpret_cast<const unsigned char *>(login.data()), login.size()) != 0) { mark_failed(); return; }
  authorization_ = "Basic " + std::string(reinterpret_cast<char *>(encoded), written);
  auth_password_.clear();
  snapshot_();
  httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
  config.httpd.max_open_sockets = 2;
  config.httpd.max_uri_handlers = 6;
  config.httpd.lru_purge_enable = true;
  config.httpd.stack_size = 8192;
  config.httpd.ctrl_port = 32769;
  config.httpd.recv_wait_timeout = 5;
  config.httpd.send_wait_timeout = 5;
  config.port_secure = 8443;
  config.servercert = reinterpret_cast<const uint8_t *>(certificate_.c_str());
  config.servercert_len = certificate_.size() + 1;
  config.prvtkey_pem = reinterpret_cast<const uint8_t *>(private_key_.c_str());
  config.prvtkey_len = private_key_.size() + 1;
  if (httpd_ssl_start(&server_, &config) != ESP_OK) { mark_failed(); ESP_LOGE(TAG, "Could not start HTTPS setup"); return; }
  httpd_uri_t icon{};icon.uri="/icon.png";icon.method=HTTP_GET;icon.handler=icon_handler_;icon.user_ctx=this;
  httpd_uri_t root{};root.uri = "/";root.method = HTTP_GET;root.handler = root_handler_;root.user_ctx = this;
  httpd_uri_t settings{};settings.uri="/setup";settings.method=HTTP_GET;settings.handler=setup_handler_;settings.user_ctx=this;
  httpd_uri_t control{};control.uri="/api/control";control.method=HTTP_POST;control.handler=control_handler_;control.user_ctx=this;
  httpd_uri_t status{};status.uri = "/api/status";status.method = HTTP_GET;status.handler = status_handler_;status.user_ctx = this;
  httpd_uri_t save{};save.uri = "/api/save";save.method = HTTP_POST;save.handler = save_handler_;save.user_ctx = this;
  if (httpd_register_uri_handler(server_, &icon) != ESP_OK || httpd_register_uri_handler(server_, &root) != ESP_OK || httpd_register_uri_handler(server_, &status) != ESP_OK || httpd_register_uri_handler(server_, &save) != ESP_OK || httpd_register_uri_handler(server_, &settings) != ESP_OK || httpd_register_uri_handler(server_, &control) != ESP_OK) {
    httpd_ssl_stop(server_);server_ = nullptr;mark_failed();return;
  }
  fingerprint_sensor_->publish_state(fingerprint_);
  url_sensor_->publish_state("https://" + App.get_name() + ".local:8443/");
}
void ClockSetup::snapshot_() {
  JsonDocument doc;
  doc["username"] = credentials_.username;
  doc["password_saved"] = credentials_.password[0] != 0;
  doc["region"] = region_select_->current_option();
  doc["revision"] = revision_;
  doc["save_status"] = save_status_;
  doc["test_status"] = connector_->test_status();
  const std::string source=source_select_->current_option();
  doc["source_status"] = source=="LibreLinkUp"||source=="Nightscout"||source=="SugarSense"?additional_->status():connector_->setup_status();
  doc["source"] = source;
  auto providers=doc["providers"].to<JsonObject>();
  auto sugar=providers["sugarsense"].to<JsonObject>();sugar["paired"]=additional_->sugar_paired();sugar["pairing"]=additional_->sugar_pairing();sugar["pair_status"]=additional_->sugar_pair_status();sugar["code"]=additional_->sugar_code();sugar["pair_url"]=additional_->sugar_url();sugar["test_status"]=additional_->test_status(additional_cgm::Provider::SUGARSENSE);
  auto dexcom=providers["dexcom"].to<JsonObject>();
  dexcom["username"]=credentials_.username;dexcom["secret_saved"]=credentials_.password[0]!=0;
  dexcom["region"]=region_select_->current_option();dexcom["test_status"]=connector_->test_status();
  auto fill=[&](const char *key,const ProviderSettings &record,additional_cgm::Provider provider){
    auto entry=providers[key].to<JsonObject>();entry["address"]=record.address;entry["secret_saved"]=record.secret[0]!=0;
    entry["region"]=record.region;entry["patient"]=record.patient;entry["mode"]=record.mode;entry["client_version"]=record.client_version;
    entry["test_status"]=additional_->test_status(provider);
  };
  fill("libre",libre_,additional_cgm::Provider::LIBRE);fill("nightscout",nightscout_,additional_cgm::Provider::NIGHTSCOUT);
  JsonDocument choices;if(!deserializeJson(choices,additional_->choices()))providers["libre"]["connections"]=choices.as<JsonArray>();
  doc["free_heap"] = esp_get_free_heap_size();
  doc["minimum_free_heap"] = esp_get_minimum_free_heap_size();
  controls_->snapshot(doc["controls"].to<JsonObject>());
  std::string json;serializeJson(doc, json);
  xSemaphoreTake(mutex_, portMAX_DELAY);snapshot_json_ = std::move(json);xSemaphoreGive(mutex_);
}
void ClockSetup::loop() {
  if (stopped_ || is_failed()) return;
  Command command;
  if (xQueueReceive(commands_, &command, 0) == pdTRUE) {
    if(std::string(command.provider)=="sugarsense") {
      if(command.forget)additional_->sugar_disconnect();else additional_->sugar_pair_start();
      save_status_="SugarSense request accepted; see pairing status below";
    } else if(std::string(command.provider)=="dexcom") {
    Credentials updated;
    if (!merge_credentials(credentials_, command.username, command.password, command.forget, updated)) {
      save_status_ = "Enter a username and a password (or keep the saved password)";
    } else if (!persist_(updated)) {
      save_status_ = "Could not save credentials; settings were not applied";
    } else {
      credentials_ = updated;
      connector_->update_credentials(credentials_.username, credentials_.password);
      if (!command.forget) {
        region_select_->make_call().set_option(command.region).perform();
        if (command.test) connector_->test_connection();
      }
      save_status_ = command.forget ? "Dexcom credentials forgotten" : command.test ? "Credentials saved; connection test requested" : "Credentials saved";
    }
    } else {
      const bool is_libre=std::string(command.provider)=="libre";
      auto &saved=is_libre?libre_:nightscout_;
      ProviderSettings input,updated;
      std::memcpy(input.address,command.username,sizeof(input.address));std::memcpy(input.secret,command.password,sizeof(input.secret));
      std::memcpy(input.region,command.region,sizeof(input.region));std::memcpy(input.patient,command.patient,sizeof(input.patient));
      std::memcpy(input.mode,command.mode,sizeof(input.mode));std::memcpy(input.client_version,command.client_version,sizeof(input.client_version));
      if(!merge_provider(saved,input,is_libre,command.forget,updated))save_status_="Enter valid provider settings and a secret (or keep the saved secret)";
      else if(!persist_provider_(is_libre?"clock_libre":"clock_ns",updated))save_status_="Could not save provider settings";
      else {
        saved=updated;auto provider=is_libre?additional_cgm::Provider::LIBRE:additional_cgm::Provider::NIGHTSCOUT;
        additional_->configure(provider,extra_config(saved));if(!command.forget&&command.test)additional_->test(provider);
        save_status_=command.forget?"Selected provider settings forgotten":command.test?"Provider settings saved; connection test requested":"Provider settings saved";
      }
    }
    ++revision_;
    snapshot_();
  }
  if ((int32_t)(millis() - next_snapshot_) >= 0) { next_snapshot_ = millis() + 1000; snapshot_(); }
}
void ClockSetup::on_shutdown() { stopped_ = true; if (server_) {httpd_ssl_stop(server_);server_ = nullptr;} }
void ClockSetup::dump_config() { ESP_LOGCONFIG(TAG, "HTTPS glucose-provider setup on port 8443; unique certificate; credentials stored in local NVS"); }

esp_err_t ClockSetup::icon_handler_(httpd_req_t *request){
  httpd_resp_set_type(request,"image/png");
  httpd_resp_set_hdr(request,"Cache-Control","public, max-age=86400");
  httpd_resp_set_hdr(request,"X-Content-Type-Options","nosniff");
  return httpd_resp_send(request,reinterpret_cast<const char *>(BRAND_ICON),BRAND_ICON_SIZE);
}
esp_err_t ClockSetup::root_handler_(httpd_req_t *request) {return page_handler_(request,false);}
esp_err_t ClockSetup::setup_handler_(httpd_req_t *request) {return page_handler_(request,true);}
esp_err_t ClockSetup::page_handler_(httpd_req_t *request,bool setup) {
  auto *self = static_cast<ClockSetup *>(request->user_ctx);
  if (!self->authenticate_(request)) return ESP_OK;
  headers_(request);
  std::string policy = "default-src 'none'; img-src 'self'; script-src 'nonce-" + self->csrf_ + "'; style-src 'nonce-" + self->csrf_ + "'; connect-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'";
  httpd_resp_set_hdr(request, "Content-Security-Policy", policy.c_str());
  httpd_resp_set_type(request, "text/html; charset=utf-8");
  std::string page = setup ? SETUP_PAGE : DASHBOARD_PAGE;
  for (size_t at = 0; (at = page.find("@TOKEN@", at)) != std::string::npos; at += self->csrf_.size()) page.replace(at, 7, self->csrf_);
  return httpd_resp_send(request, page.c_str(), page.size());
}
esp_err_t ClockSetup::status_handler_(httpd_req_t *request) {
  auto *self = static_cast<ClockSetup *>(request->user_ctx);
  if (!self->authenticate_(request)) return ESP_OK;
  headers_(request);httpd_resp_set_type(request, "application/json");
  xSemaphoreTake(self->mutex_, portMAX_DELAY);std::string body = self->snapshot_json_;xSemaphoreGive(self->mutex_);
  return httpd_resp_send(request, body.c_str(), body.size());
}
esp_err_t ClockSetup::control_handler_(httpd_req_t *request) {
  auto *self=static_cast<ClockSetup*>(request->user_ctx);if(!self->authenticate_(request))return ESP_OK;
  char token[80]{},type[80]{};
  if(httpd_req_get_hdr_value_str(request,"X-CSRF-Token",token,sizeof(token))!=ESP_OK||!constant_equal(token,self->csrf_))return error_(request,"403 Forbidden","Reload the control page before saving");
  if(httpd_req_get_hdr_value_str(request,"Content-Type",type,sizeof(type))!=ESP_OK||std::string(type)!="application/json")return error_(request,"415 Unsupported Media Type","JSON required");
  if(request->content_len<=0||request->content_len>2048)return error_(request,"413 Content Too Large","Control request too large");
  std::string body(request->content_len,'\0');size_t offset=0;
  while(offset<body.size()){int count=httpd_req_recv(request,body.data()+offset,body.size()-offset);if(count<=0)return error_(request,"408 Request Timeout","Incomplete control request");offset+=count;}
  int result=self->controls_->enqueue(body);
  if(result!=202)return error_(request,result==400?"400 Bad Request":"409 Conflict",result==400?"Invalid control or value":"Controls are busy; wait and try again");
  headers_(request);httpd_resp_set_status(request,"202 Accepted");httpd_resp_set_type(request,"application/json");return httpd_resp_sendstr(request,"{\"accepted\":true}");
}
esp_err_t ClockSetup::save_handler_(httpd_req_t *request) {
  auto *self = static_cast<ClockSetup *>(request->user_ctx);
  if (!self->authenticate_(request)) return ESP_OK;
  char token[80]{};
  if (httpd_req_get_hdr_value_str(request, "X-CSRF-Token", token, sizeof(token)) != ESP_OK || !constant_equal(token, self->csrf_))
    return error_(request, "403 Forbidden", "Reload the setup page before saving");
  char type[80]{};
  if (httpd_req_get_hdr_value_str(request, "Content-Type", type, sizeof(type)) != ESP_OK || std::string(type) != "application/json")
    return error_(request, "415 Unsupported Media Type", "JSON required");
  if (request->content_len <= 0 || request->content_len > 4096) return error_(request, "413 Content Too Large", "Settings request too large");
  std::string body(request->content_len, '\0');size_t offset = 0;
  while (offset < body.size()) {
    int count = httpd_req_recv(request, body.data() + offset, body.size() - offset);
    if (count <= 0) return error_(request, "408 Request Timeout", "Incomplete settings request");
    offset += count;
  }
  JsonDocument doc;
  if (deserializeJson(doc, body) || !doc.is<JsonObject>() || !doc["forget"].is<bool>()) return error_(request, "400 Bad Request", "Invalid settings");
  if (!doc["test"].isNull() && !doc["test"].is<bool>()) return error_(request, "400 Bad Request", "Invalid test setting");
  Command command;command.forget=doc["forget"].as<bool>();command.test=doc["test"]|true;
  if(!doc["provider"].isNull()&&!doc["provider"].is<const char *>())return error_(request,"400 Bad Request","Invalid provider");
  std::string provider=doc["provider"]|"dexcom";
  if(provider!="dexcom"&&provider!="libre"&&provider!="nightscout"&&provider!="sugarsense")return error_(request,"400 Bad Request","Invalid provider");
  std::memset(command.provider,0,sizeof(command.provider));std::copy(provider.begin(),provider.end(),command.provider);
  if(!command.forget&&provider!="sugarsense") {
    if(!doc["username"].is<const char *>()||!doc["password"].is<const char *>()||!doc["region"].is<const char *>())return error_(request,"400 Bad Request","Missing settings fields");
    std::string user=doc["username"].as<std::string>(),secret=doc["password"].as<std::string>(),region=doc["region"].as<std::string>();
    if(!valid_field(user)||user.empty()||!valid_field(secret)||region.size()>15)return error_(request,"400 Bad Request","Invalid settings length");
    if(provider=="dexcom"&&(region!="US"&&region!="Outside US"&&region!="Japan"))return error_(request,"400 Bad Request","Invalid Dexcom region");
    std::copy(user.begin(),user.end(),command.username);std::copy(secret.begin(),secret.end(),command.password);std::copy(region.begin(),region.end(),command.region);
    if(provider!="dexcom") {
      auto copy=[&](const char *key,char *target,size_t capacity){if(!doc[key].is<const char *>())return false;std::string value=doc[key].as<std::string>();if(value.size()>=capacity)return false;std::memset(target,0,capacity);std::copy(value.begin(),value.end(),target);return true;};
      if(!copy("patient",command.patient,sizeof(command.patient))||!copy("mode",command.mode,sizeof(command.mode))||!copy("client_version",command.client_version,sizeof(command.client_version)))return error_(request,"400 Bad Request","Missing provider fields");
      ProviderSettings input;
      std::memcpy(input.address,command.username,sizeof(input.address));std::memcpy(input.secret,command.password,sizeof(input.secret));std::memcpy(input.region,command.region,sizeof(input.region));
      std::memcpy(input.patient,command.patient,sizeof(input.patient));std::memcpy(input.mode,command.mode,sizeof(input.mode));std::memcpy(input.client_version,command.client_version,sizeof(input.client_version));
      if(!valid_provider_record(input,provider=="libre"))return error_(request,"400 Bad Request","Invalid provider settings; Nightscout requires an HTTPS base URL");
    }
  }
  if (xQueueSend(self->commands_, &command, 0) != pdTRUE) return error_(request, "409 Conflict", "Settings are already being applied; wait and try again");
  headers_(request);httpd_resp_set_status(request, "202 Accepted");httpd_resp_set_type(request, "application/json");
  return httpd_resp_sendstr(request, "{\"accepted\":true}");
}
}  // namespace esphome::clock_setup

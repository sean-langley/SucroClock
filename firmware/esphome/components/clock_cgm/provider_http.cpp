#include "provider_http.h"
#ifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha1.h>
namespace esphome::clock_cgm {
namespace {
SemaphoreHandle_t work_mutex=nullptr;
struct Body {std::string value;size_t limit;bool overflow{false};};
esp_err_t receive(esp_http_client_event_t *event){
 if(event->event_id!=HTTP_EVENT_ON_DATA)return ESP_OK;
 auto *body=static_cast<Body*>(event->user_data);
 if(event->data_len<0||body->value.size()+event->data_len>body->limit){body->overflow=true;return ESP_FAIL;}
 body->value.append(static_cast<const char*>(event->data),event->data_len);return ESP_OK;
}
std::string hex(const uint8_t *bytes,size_t count){const char *digits="0123456789abcdef";std::string result;result.reserve(count*2);for(size_t i=0;i<count;++i){result+=digits[bytes[i]>>4];result+=digits[bytes[i]&15];}return result;}
}
bool init_provider_lock(){if(!work_mutex)work_mutex=xSemaphoreCreateMutex();return work_mutex!=nullptr;}
ProviderWorkGuard::ProviderWorkGuard(){locked_=work_mutex&&xSemaphoreTake(work_mutex,portMAX_DELAY)==pdTRUE;}
ProviderWorkGuard::~ProviderWorkGuard(){if(locked_)xSemaphoreGive(work_mutex);}
Reply https_request(const std::string &method,const std::string &url,const Headers &headers,const std::string &payload,size_t limit){
 if(url.compare(0,8,"https://")!=0)return {};
 Body body{{},limit,false};esp_http_client_config_t config{};
 config.url=url.c_str();config.method=method=="GET"?HTTP_METHOD_GET:HTTP_METHOD_POST;config.timeout_ms=8000;
 config.crt_bundle_attach=esp_crt_bundle_attach;config.disable_auto_redirect=true;config.event_handler=receive;config.user_data=&body;
 auto client=esp_http_client_init(&config);if(!client)return {};
 for(const auto &header:headers)esp_http_client_set_header(client,header.first.c_str(),header.second.c_str());
 if(method!="GET")esp_http_client_set_post_field(client,payload.data(),payload.size());
 auto error=esp_http_client_perform(client);int status=error==ESP_OK&&!body.overflow?esp_http_client_get_status_code(client):-1;
 esp_http_client_cleanup(client);return {status,std::move(body.value)};
}
std::string SecureProviderTransport::sha256(const std::string &value){uint8_t out[32];if(mbedtls_sha256(reinterpret_cast<const uint8_t*>(value.data()),value.size(),out,0)!=0)return "";return hex(out,32);}
std::string SecureProviderTransport::sha1(const std::string &value){uint8_t out[20];if(mbedtls_sha1(reinterpret_cast<const uint8_t*>(value.data()),value.size(),out)!=0)return "";return hex(out,20);}
}
#else
namespace esphome::clock_cgm {
bool init_provider_lock(){return true;}
ProviderWorkGuard::ProviderWorkGuard(){locked_=true;}ProviderWorkGuard::~ProviderWorkGuard()=default;
Reply https_request(const std::string&,const std::string&,const Headers&,const std::string&,size_t){return {};}
std::string SecureProviderTransport::sha256(const std::string&){return "";}
std::string SecureProviderTransport::sha1(const std::string&){return "";}
}
#endif

#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <cmath>
namespace esphome {
inline uint32_t fake_millis = 1000;
inline bool fake_wifi = true;
inline bool fake_task_success = true;
inline uint32_t millis() { return fake_millis; }
class Component { public: virtual ~Component()=default; virtual void setup(){}; virtual void loop(){}; virtual void on_shutdown(){}; virtual void dump_config(){}; };
namespace network { inline bool is_connected() { return fake_wifi; } }
namespace sensor { class Sensor { public: float state=NAN; bool known=false; unsigned publications=0; bool has_state() const {return known;} void publish_state(float x) {state=x;known=true;++publications;} }; }
namespace text_sensor { class TextSensor { public: std::string state; bool known=false; bool has_state() const {return known;} void publish_state(const std::string &x){state=x;known=true;} }; }
namespace binary_sensor { class BinarySensor { public: bool state=false,known=false; bool has_state() const{return known;} void publish_state(bool x){state=x;known=true;} }; }
namespace aip33628 { class Aip33628Panel { public: bool provider=false,manual_override=false; unsigned updates=0; int number=0; uint8_t owner=0; void clear_provider_number(uint8_t requested=1){if(owner==requested){provider=false;owner=0;}} bool show_provider_number(int v,const std::string &,uint8_t requested=1){if(manual_override)return false;provider=true;owner=requested;number=v;++updates;return true;} }; }
}
#define ESP_LOGCONFIG(tag, ...) ((void)(tag))
struct FakeQueue { size_t size,capacity; std::deque<std::vector<unsigned char>> items; };
using QueueHandle_t=FakeQueue*;
using TaskHandle_t=void*;
constexpr int pdTRUE=1,pdPASS=1;
constexpr uint32_t portMAX_DELAY=0xffffffff;
inline QueueHandle_t xQueueCreate(size_t n,size_t size){return new FakeQueue{size,n,{}};}
inline int xQueueSend(QueueHandle_t q,const void *p,uint32_t){if(q->items.size()==q->capacity)return 0;auto &v=q->items.emplace_back(q->size);std::memcpy(v.data(),p,q->size);return 1;}
inline int xQueueReceive(QueueHandle_t q,void *p,uint32_t){if(q->items.empty())return 0;std::memcpy(p,q->items.front().data(),q->size);q->items.pop_front();return 1;}
inline int xTaskCreate(void(*)(void*),const char*,int,void*,int,TaskHandle_t *handle){if(!esphome::fake_task_success)return 0;*handle=reinterpret_cast<void*>(1);return 1;}
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1,HTTP_EVENT_ON_DATA=1,HTTP_METHOD_POST=1;
struct esp_http_client_event_t {int event_id;void *user_data;int data_len;void *data;};
struct esp_http_client_config_t {const char *url;int method,timeout_ms;int(*crt_bundle_attach)(void*);bool disable_auto_redirect;int(*event_handler)(esp_http_client_event_t*);void *user_data;};
inline int esp_crt_bundle_attach(void*) {return 0;}
inline void *esp_http_client_init(esp_http_client_config_t*){return nullptr;}
inline void esp_http_client_set_header(void*,const char*,const char*){}
inline void esp_http_client_set_post_field(void*,const char*,size_t){}
inline int esp_http_client_perform(void*){return -1;}
inline int esp_http_client_get_status_code(void*){return 0;}
inline void esp_http_client_cleanup(void*){}

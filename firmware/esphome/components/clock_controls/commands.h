#pragma once
#include <ArduinoJson.h>
#include "glucose_style.h"
#include <string>
#include <cmath>
#include <cstring>
namespace esphome::clock_controls {
struct Command {GlucoseStyle style;char kind[12]{},id[32]{},text[65]{};double value{0},brightness{0};bool state{false};unsigned red{0},green{0},blue{0};};
inline bool one_of(const std::string &value,std::initializer_list<const char*> values){for(auto option:values)if(value==option)return true;return false;}
inline bool parse_command(const std::string &body,Command &c){
 JsonDocument doc;if(body.size()>2048||deserializeJson(doc,body)||!doc.is<JsonObject>()||!doc["kind"].is<const char*>())return false;
 std::string kind=doc["kind"].as<std::string>(),id=doc["id"]|"";
 if(kind.size()>=sizeof(c.kind)||id.size()>=sizeof(c.id))return false;
 if(kind=="retry"||kind=="style_reset"){}
 else if(kind=="style"){
  if(!doc["enabled"].is<bool>()||!doc["smooth"].is<bool>()||!doc["breathing"].is<bool>()||!doc["rapid"].is<bool>()||
     !doc["low"].is<int>()||!doc["target_high"].is<int>()||!doc["high"].is<int>()||!doc["blend_width"].is<double>()||
     !doc["alert_seconds"].is<int>()||!doc["period_seconds"].is<int>()||!doc["colors"].is<JsonArray>()||doc["colors"].size()!=5)return false;
  int low=doc["low"].as<int>(),target=doc["target_high"].as<int>(),high=doc["high"].as<int>();
  int duration=doc["alert_seconds"].as<int>(),period=doc["period_seconds"].as<int>();
  if(low<20||high>600||low>=target||target>=high||duration<0||duration>120||period<2||period>30)return false;
  c.style.low=low;c.style.target_high=target;c.style.high=high;c.style.enabled=doc["enabled"].as<bool>();c.style.smooth=doc["smooth"].as<bool>();
  c.style.breathing=doc["breathing"].as<bool>();c.style.rapid=doc["rapid"].as<bool>();c.style.blend_width=doc["blend_width"].as<float>();
  c.style.alert_seconds=duration;c.style.period_seconds=period;
  for(size_t i=0;i<5;++i){if(!doc["colors"][i].is<const char*>()||!parse_hex(doc["colors"][i].as<std::string>(),c.style.colors[i]))return false;}
  if(!valid_style(c.style))return false;
 }
 else if(kind=="light"){
  if(!doc["state"].is<bool>()||!doc["brightness"].is<double>()||!doc["color"].is<const char*>())return false;
  c.state=doc["state"].as<bool>();c.brightness=doc["brightness"].as<double>();std::string color=doc["color"].as<std::string>();
  if(!std::isfinite(c.brightness)||c.brightness<0||c.brightness>100||color.size()!=7||color[0]!='#')return false;
  unsigned value=0;for(size_t i=1;i<7;++i){char ch=color[i];unsigned n=ch>='0'&&ch<='9'?ch-'0':ch>='a'&&ch<='f'?ch-'a'+10:ch>='A'&&ch<='F'?ch-'A'+10:99;if(n>15)return false;value=value*16+n;}
  c.red=(value>>16)&255;c.green=(value>>8)&255;c.blue=value&255;
 }
 else if(kind=="switch"){
  if(!one_of(id,{"twelve_hour","blink"})||!doc["value"].is<bool>())return false;c.state=doc["value"].as<bool>();
 }
 else if(kind=="number"){
  if(!one_of(id,{"speed","angle","hue_span","flash_fade","test_number"})||!doc["value"].is<double>())return false;
  c.value=doc["value"].as<double>();if(!std::isfinite(c.value)||c.value < -10000||c.value>10000)return false;
 }
 else if(kind=="select"){
  if(!doc["value"].is<const char*>())return false;std::string value=doc["value"].as<std::string>();bool valid=false;
  if(id=="source")valid=one_of(value,{"Home Assistant","Dexcom Share","LibreLinkUp","Nightscout","SugarSense"});
  else if(id=="mode")valid=one_of(value,{"Preview","Display"});
  else if(id=="effect")valid=one_of(value,{"None","Color Cycle","Flash"});
  else if(id=="spread")valid=one_of(value,{"Whole Panel","Per Digit","Per LED"});
  else if(id=="unit")valid=one_of(value,{"None","F","C"});
  else if(id=="region")valid=one_of(value,{"US","Outside US","Japan"});
  if(!valid||value.size()>=sizeof(c.text))return false;std::memcpy(c.text,value.c_str(),value.size()+1);
 }
 else if(kind=="button"){if(!one_of(id,{"lamp_test","seconds","digit_demo","clear_colors","gradient"}))return false;}
 else return false;
 std::memcpy(c.kind,kind.c_str(),kind.size()+1);std::memcpy(c.id,id.c_str(),id.size()+1);return true;
}
}

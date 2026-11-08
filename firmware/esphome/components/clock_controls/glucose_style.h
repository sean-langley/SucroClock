#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <string>
namespace esphome::clock_controls {
struct RGB {float r{0},g{0},b{0};};
struct GlucoseStyle {
  uint32_t version{1};
  uint16_t low{70},target_high{180},high{250};
  uint8_t enabled{1},smooth{0},breathing{1},rapid{1};
  float blend_width{10.0f};
  uint16_t alert_seconds{15},period_seconds{4};
  uint32_t colors[5]{0xFF0000,0x00C000,0xFFC000,0xFF0000,0xFF0000};
};
inline bool valid_style(const GlucoseStyle &s){
 if(s.version!=1||s.low<20||s.high>600||s.low>=s.target_high||s.target_high>=s.high||s.enabled>1||s.smooth>1||s.breathing>1||s.rapid>1)return false;
 if(!std::isfinite(s.blend_width)||s.blend_width<0.1f||s.blend_width>50||s.alert_seconds>120||s.period_seconds<2||s.period_seconds>30)return false;
 if(s.smooth&&s.blend_width>std::min(s.target_high-s.low,s.high-s.target_high)/2.0f)return false;
 for(uint32_t color:s.colors)if(color>0xFFFFFF)return false;return true;
}
inline RGB rgb(uint32_t color){return {float((color>>16)&255)/255.0f,float((color>>8)&255)/255.0f,float(color&255)/255.0f};}
inline RGB mix(RGB a,RGB b,float t){t=std::max(0.0f,std::min(1.0f,t));return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};}
inline int glucose_band(const GlucoseStyle &s,int value){return value<s.low?0:value<=s.target_high?1:value<=s.high?2:3;}
inline RGB glucose_color(const GlucoseStyle &s,int value){
 if(!s.smooth)return rgb(s.colors[glucose_band(s,value)]);
 float width=s.blend_width;
 if(value<s.low)return rgb(s.colors[0]);
 if(value<s.low+width)return mix(rgb(s.colors[0]),rgb(s.colors[1]),(value-s.low)/width);
 if(value<s.target_high-width/2)return rgb(s.colors[1]);
 if(value<s.target_high+width/2)return mix(rgb(s.colors[1]),rgb(s.colors[2]),(value-s.target_high+width/2)/width);
 if(value<s.high-width)return rgb(s.colors[2]);
 if(value<s.high)return mix(rgb(s.colors[2]),rgb(s.colors[3]),(value-s.high+width)/width);
 return rgb(s.colors[3]);
}
inline bool parse_hex(const std::string &s,uint32_t &color){
 if(s.size()!=7||s[0]!='#')return false;color=0;
 for(size_t i=1;i<7;++i){char c=s[i];unsigned n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:99;if(n>15)return false;color=color*16+n;}return true;
}
struct StyleFrame {RGB color;float envelope{1};bool active{false},alert{false},breathing{false};int band{-1};};
class StyleEngine {
 public:
  void reset(){latched_=false;started_=0;context_.clear();}
  StyleFrame update(const GlucoseStyle &config,bool direct_display,bool fresh,int value,bool rapid,const std::string &source,uint32_t now){
    if(source!=context_){latched_=false;context_=source;}
    StyleFrame frame;
    if(!config.enabled||!direct_display||!fresh){latched_=false;return frame;}
    frame.active=true;frame.band=glucose_band(config,value);frame.color=glucose_color(config,value);
    frame.alert=value<config.low||value>config.high||(config.rapid&&rapid);
    if(frame.alert){
      if(!latched_){latched_=true;started_=now;}
      if(config.rapid&&rapid)frame.color=rgb(config.colors[4]);
      uint32_t elapsed=now-started_;
      if(config.breathing&&elapsed<uint32_t(config.alert_seconds)*1000U){
        frame.breathing=true;float phase=std::fmod(elapsed/1000.0f,float(config.period_seconds));float half=config.period_seconds/2.0f;
        frame.envelope=phase<half?phase/half:1.0f-(phase-half)/half;
      }
    }else latched_=false;
    return frame;
  }
 protected:bool latched_{false};uint32_t started_{0};std::string context_;
};
}

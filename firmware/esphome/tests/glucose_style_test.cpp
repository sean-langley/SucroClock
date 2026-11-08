#include "esphome/components/clock_controls/commands.h"
#include <cassert>
#include <iostream>
using namespace esphome::clock_controls;
static bool near(float a,float b){return std::abs(a-b)<0.001f;}
int main(){
 GlucoseStyle s;assert(valid_style(s));
 int values[]{69,70,180,181,250,251};int bands[]{0,1,1,2,2,3};
 for(int i=0;i<6;i++){assert(glucose_band(s,values[i])==bands[i]);auto a=glucose_color(s,values[i]),b=rgb(s.colors[bands[i]]);assert(near(a.r,b.r)&&near(a.g,b.g)&&near(a.b,b.b));}
 StyleEngine e;for(unsigned i=0;i<=4;i++){auto f=e.update(s,true,true,60,false,"Dexcom",100+i*1000);float expected[]{0,.5,1,.5,0};assert(f.active&&f.alert&&f.breathing&&near(f.envelope,expected[i]));}
 assert(!e.update(s,true,true,60,false,"Dexcom",15100).breathing);assert(!e.update(s,true,true,60,false,"Dexcom",30100).breathing);
 assert(!e.update(s,true,true,100,false,"Dexcom",31000).alert);assert(e.update(s,true,true,100,true,"Dexcom",32000).breathing);
 assert(!e.update(s,true,false,60,false,"Dexcom",33000).active);assert(e.update(s,true,true,60,false,"Dexcom",34000).breathing);
 assert(!e.update(s,false,true,60,false,"Dexcom",35000).active);assert(e.update(s,true,true,60,false,"Libre",36000).breathing);
 s.enabled=0;assert(!e.update(s,true,true,60,false,"Libre",37000).active);s.enabled=1;
 e.reset();e.update(s,true,true,60,false,"x",0xfffffff0U);assert(near(e.update(s,true,true,60,false,"x",984).envelope,.5));
 s.colors[0]=0x0000ff;e.reset();auto custom=e.update(s,true,true,60,false,"x",39000);assert(near(custom.color.b,1)&&near(custom.color.r,0));
 s.rapid=0;assert(!e.update(s,true,true,100,true,"x",40000).alert);
 s=GlucoseStyle{};s.smooth=1;auto c=glucose_color(s,75);assert(near(c.r,.5)&&near(c.g,96.0f/255));assert(near(glucose_color(s,80).g,192.0f/255));
 s.low=179;assert(!valid_style(s));s=GlucoseStyle{};s.high=s.target_high;assert(!valid_style(s));s=GlucoseStyle{};s.colors[0]=0x1000000;assert(!valid_style(s));
 uint32_t color;assert(parse_hex("#aBcD01",color)&&color==0xabcd01);assert(!parse_hex("#gg0000",color));assert(!parse_hex("ff0000",color));
 const std::string valid=R"({"kind":"style","enabled":true,"smooth":false,"breathing":true,"rapid":true,"low":70,"target_high":180,"high":250,"blend_width":10,"alert_seconds":15,"period_seconds":4,"colors":["#ff0000","#00c000","#ffc000","#ff0000","#ff0000"]})";
 Command command;assert(parse_command(valid,command));assert(command.style.target_high==180);assert(parse_command(R"({"kind":"style_reset"})",command));
 for(auto pair:{std::pair<std::string,std::string>{"\"low\":70","\"low\":180"},{"\"rapid\":true","\"rapid\":1"},{"#00c000","#xxxxxx"},{"\"period_seconds\":4","\"period_seconds\":0"},{"\"blend_width\":10","\"blend_width\":99"}}){auto bad=valid;bad.replace(bad.find(pair.first),pair.first.size(),pair.second);assert(!parse_command(bad,command));}
 assert(!parse_command(R"({"kind":"style"})",command));
 std::cout<<"Glucose boundary colors, blending, alert timing, rollover, persistence validation and command tests passed.\n";
}

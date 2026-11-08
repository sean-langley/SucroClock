#include "esphome/components/clock_controls/commands.h"
#include <cassert>
#include <iostream>
using namespace esphome::clock_controls;
int main(){
 Command c;
 assert(parse_command(R"({"kind":"light","state":true,"brightness":42,"color":"#aBcD01"})",c));assert(c.state&&c.brightness==42&&c.red==171&&c.green==205&&c.blue==1);
 for(const char *s:{R"({"kind":"select","id":"source","value":"SugarSense"})",R"({"kind":"select","id":"source","value":"Nightscout"})",R"({"kind":"select","id":"mode","value":"Preview"})",R"({"kind":"switch","id":"blink","value":false})",R"({"kind":"number","id":"angle","value":-45})",R"({"kind":"button","id":"lamp_test"})",R"({"kind":"retry"})"})assert(parse_command(s,c));
 for(const char *s:{"bad JSON","[]",R"({"kind":"unknown"})",R"({"kind":"select","id":"password","value":"x"})",R"({"kind":"select","id":"source","value":"unknown"})",R"({"kind":"select","id":"mode","value":"Anything"})",R"({"kind":"switch","id":"blink","value":"true"})",R"({"kind":"number","id":"speed","value":1e309})",R"({"kind":"number","id":"speed","value":"4"})",R"({"kind":"button","id":"erase_flash"})",R"({"kind":"light","state":true,"brightness":101,"color":"#ffffff"})",R"({"kind":"light","state":true,"brightness":50,"color":"javascript:x"})",R"({"kind":"light","state":false,"brightness":50,"color":"#gggggg"})"})assert(!parse_command(s,c));
 assert(!parse_command(std::string(2049,'x'),c));
 std::cout<<"Control command allowlist, data types, bounds and invalid-action tests passed.\n";
}

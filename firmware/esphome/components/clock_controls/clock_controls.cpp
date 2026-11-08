#include "clock_controls.h"
#include <cstdio>
#include <nvs.h>
namespace esphome::clock_controls {
void ClockControls::setup(){
 commands_=xQueueCreate(3,sizeof(Command));if(!commands_){mark_failed();return;}
 nvs_handle_t handle;
 if(nvs_open("clock_colors",NVS_READONLY,&handle)==ESP_OK){GlucoseStyle loaded;size_t size=sizeof(loaded);auto error=nvs_get_blob(handle,"settings",&loaded,&size);nvs_close(handle);if(error==ESP_OK&&size==sizeof(loaded)&&valid_style(loaded))style_=loaded;}
}
bool ClockControls::persist_style_(const GlucoseStyle &config){
 nvs_handle_t handle;if(nvs_open("clock_colors",NVS_READWRITE,&handle)!=ESP_OK)return false;
 auto error=nvs_set_blob(handle,"settings",&config,sizeof(config));if(error==ESP_OK)error=nvs_commit(handle);nvs_close(handle);return error==ESP_OK;
}
void ClockControls::update_style_(){
 std::string source=selects_.at("source")->current_option();std::string prefix=source=="Dexcom Share"?"dx":"other";
 float value=sensors_.at(prefix+"_glucose")->state;bool fresh=binary_.at(prefix+"_fresh")->state&&std::isfinite(value);
 std::string trend=texts_.at(prefix+"_trend")->state;
 bool direct=source!="Home Assistant"&&selects_.at("mode")->current_option()=="Display";
 panel_->set_direct_display(direct);
 style_frame_=style_engine_.update(style_,direct,fresh,std::isfinite(value)?int(std::lround(value)):0,trend=="Double up"||trend=="Double down",source,millis());
 panel_->set_glucose_color(style_frame_.active,style_frame_.color.r,style_frame_.color.g,style_frame_.color.b,style_frame_.envelope);
}
int ClockControls::enqueue(const std::string &body){Command command;if(!parse_command(body,command))return 400;if(!commands_||xQueueSend(commands_,&command,0)!=pdTRUE)return 409;return 202;}
void ClockControls::loop(){
 Command command;if(commands_&&xQueueReceive(commands_,&command,0)==pdTRUE)apply_command_(command);
 if((int32_t)(millis()-next_style_)>=0){next_style_=millis()+40;update_style_();}
}
void ClockControls::apply_command_(const Command &c){
 ++revision_;std::string kind=c.kind,id=c.id;message_="Control updated";
 if(kind=="style"||kind=="style_reset"){
  GlucoseStyle config=kind=="style_reset"?GlucoseStyle{}:c.style;
  if(!persist_style_(config))message_="Could not save glucose colors; previous settings retained";
  else {style_=config;message_="Glucose ranges and colors saved";update_style_();}
 }
 else if(kind=="light"){light_->make_call().set_state(c.state).set_brightness(c.brightness/100.0f).set_rgb(c.red/255.0f,c.green/255.0f,c.blue/255.0f).perform();}
 else if(kind=="switch"){auto it=switches_.find(id);if(it!=switches_.end())it->second->control(c.state);}
 else if(kind=="select"){auto it=selects_.find(id);if(it!=selects_.end())it->second->make_call().set_option(c.text).perform();}
 else if(kind=="number"){
  auto it=numbers_.find(id);if(it!=numbers_.end()){auto &traits=it->second->traits;if(c.value<traits.get_min_value()||c.value>traits.get_max_value())message_="Value outside the allowed range";else it->second->make_call().set_value(c.value).perform();}
 }
 else if(kind=="button"){auto it=buttons_.find(id);if(it!=buttons_.end())it->second->press();}
 else if(kind=="retry"){std::string source=selects_.at("source")->current_option();if(source=="Dexcom Share")cgm_->retry();else if(source=="LibreLinkUp"||source=="Nightscout"||source=="SugarSense")other_->retry();else message_="Home Assistant is the selected source";}
}
void ClockControls::snapshot(JsonObject root){
 auto policy=root["style"].to<JsonObject>();policy["enabled"]=bool(style_.enabled);policy["smooth"]=bool(style_.smooth);
 policy["breathing"]=bool(style_.breathing);policy["rapid"]=bool(style_.rapid);policy["low"]=style_.low;policy["target_high"]=style_.target_high;policy["high"]=style_.high;
 policy["blend_width"]=style_.blend_width;policy["alert_seconds"]=style_.alert_seconds;policy["period_seconds"]=style_.period_seconds;
 auto colors=policy["colors"].to<JsonArray>();for(uint32_t color:style_.colors){char text[8];std::snprintf(text,sizeof(text),"#%06x",unsigned(color));colors.add(text);}
 bool applied;float red,green,blue,envelope;panel_->glucose_color_state(applied,red,green,blue,envelope);
 std::string reason = !style_.enabled ? "Glucose colors are switched off."
  : selects_.at("source")->current_option()=="Home Assistant" ? "Home Assistant is selected; its automation controls colors."
  : selects_.at("mode")->current_option()!="Display" ? "Preview mode: select Display to show glucose on the clock."
  : !style_frame_.active ? "Waiting for a fresh reading from the selected provider."
  : !applied ? "A temporary clock display is active; glucose colors resume afterward."
  : "";
 policy["inactive_reason"]=reason;
 policy["applied"]=applied;policy["alert_active"]=style_frame_.alert;policy["breathing_active"]=style_frame_.breathing;policy["band"]=style_frame_.band;policy["envelope"]=envelope;
 char applied_color[8];std::snprintf(applied_color,sizeof(applied_color),"#%02x%02x%02x",unsigned(std::lround(red*255)),unsigned(std::lround(green*255)),unsigned(std::lround(blue*255)));policy["applied_color"]=applied_color;
 root["revision"]=revision_;root["message"]=message_;
 auto light=root["light"].to<JsonObject>();const auto &v=light_->remote_values;light["state"]=v.is_on();light["brightness"]=int(std::lround(v.get_brightness()*100));
 char color[8];std::snprintf(color,sizeof(color),"#%02x%02x%02x",int(v.get_red()*255),int(v.get_green()*255),int(v.get_blue()*255));light["color"]=color;
 for(const auto &entry:switches_)root["switches"][entry.first]=entry.second->state;
 for(const auto &entry:selects_){auto value=root["selects"][entry.first].to<JsonObject>();value["value"]=entry.second->current_option();auto options=value["options"].to<JsonArray>();for(const auto &option:entry.second->traits.get_options())options.add(option);}
 for(const auto &entry:numbers_){auto value=root["numbers"][entry.first].to<JsonObject>();value["value"]=entry.second->state;value["min"]=entry.second->traits.get_min_value();value["max"]=entry.second->traits.get_max_value();value["step"]=entry.second->traits.get_step();}
 for(const auto &entry:binary_)root["buttons"][entry.first]=entry.second->state;
 std::string source=selects_.at("source")->current_option();std::string prefix=source=="Dexcom Share"?"dx":"other";bool direct=source!="Home Assistant";
 auto reading=root["reading"].to<JsonObject>();reading["fresh"]=direct&&binary_.at(prefix+"_fresh")->state;
 if(direct){reading["value"]=sensors_.at(prefix+"_glucose")->state;reading["age"]=sensors_.at(prefix+"_age")->state;reading["trend"]=texts_.at(prefix+"_trend")->state;reading["time"]=texts_.at(prefix+"_time")->state;}
 reading["status"]=source=="Dexcom Share"?cgm_->setup_status():source=="Home Assistant"?std::string("Managed by Home Assistant"):other_->status();
}
}

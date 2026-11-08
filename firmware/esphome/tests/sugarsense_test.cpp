#include "esphome/components/clock_cgm/sugarsense_client.h"
#include "esphome/components/clock_setup/settings.h"
#include <ArduinoJson.h>
#include <cassert>
#include <deque>
#include <iostream>
using namespace esphome::clock_cgm;
class Transport:public ProviderTransport {
 public:
 std::deque<Reply> replies;std::string path,payload;Headers sent;
 Reply request(const std::string &method,const std::string &url,const Headers &headers,const std::string &body)override{assert(method=="POST"&&url.find(SUGAR_API)==0);path=url;payload=body;sent=headers;assert(!replies.empty());auto r=replies.front();replies.pop_front();return r;}
 std::string sha256(const std::string&)override{return "";}std::string sha1(const std::string&)override{return "";}
 void reply(std::string body,int status=200){replies.push_back({status,body});}
};
int main(){
 Transport t;const std::string token(43,'a');assert(valid_sugar_token(token));assert(!valid_sugar_token(std::string(43,'\n')));assert(!valid_sugar_token(token+"a"));
 t.reply(R"({"response":{"code":"ABC123","pollSecret":"private-secret","expiresInSeconds":600,"pollIntervalSeconds":5,"pairUrl":"https://evil.example/"}})");auto p=sugar_pair_start(t);assert(p.outcome==FetchOutcome::OK&&p.pair.url=="https://app.sugarsense.io/display?c=ABC123");for(auto &h:t.sent)assert(h.first!="Authorization");
 t.reply(R"({"response":{"pending":true}})");auto poll=sugar_pair_poll(p.pair,"disp-001122334455",t);assert(poll.pending&&poll.token.empty());JsonDocument body;assert(!deserializeJson(body,t.payload));assert(body["pollSecret"]=="private-secret"&&body["deviceId"]=="disp-001122334455"&&body["model"]=="SucroClock HU-058D");
 t.reply("{\"response\":{\"pending\":false,\"displayToken\":\""+token+"\"}}");poll=sugar_pair_poll(p.pair,"disp-001122334455",t);assert(!poll.pending&&poll.token==token);
 t.reply("{}",404);assert(sugar_pair_poll(p.pair,"d",t).outcome==FetchOutcome::SESSION);
 t.reply(R"({"response":{"pending":false,"displayToken":"short"}})");assert(sugar_pair_poll(p.pair,"d",t).outcome==FetchOutcome::INVALID_RESPONSE);
 t.reply(R"({"response":{"code":"ABC123","pollSecret":"x","expiresInSeconds":999999,"pollIntervalSeconds":0}})");p=sugar_pair_start(t);assert(p.pair.expires_s==600&&p.pair.poll_s==2);
 t.reply(R"({"response":{"code":"<bad>","pollSecret":"x"}})");assert(sugar_pair_start(t).outcome==FetchOutcome::INVALID_RESPONSE);
 ExtraConfig config;config.secret=token;
 const int trends[]{0,6,5,4,3,2};for(int a=0;a<=6;a++){
 t.reply("{\"response\":{\"state\":0,\"value\":123,\"date\":1794080000000,\"arrowDirection\":"+std::to_string(a)+",\"isMmol\":true,\"pullSeconds\":900}}");auto r=fetch_sugarsense(config,t);assert(r.outcome==FetchOutcome::OK&&r.reading.mg_dl==123&&r.reading.measured_at==1794080000&&r.reading.trend==(a<=5?trends[a]:0)&&r.pull_seconds==900);bool auth=false;for(auto &h:t.sent)if(h.first=="Authorization")auth=h.second=="Bearer "+token;assert(auth);assert(!deserializeJson(body,t.payload)&&body["hours"]==0);
 }
 for(int status:{401,403,429,500,302}){t.reply("{}",status);auto result=fetch_sugarsense(config,t);assert(result.outcome==(status==401||status==403?FetchOutcome::AUTH:status==429?FetchOutcome::RATE_LIMIT:status==500?FetchOutcome::NETWORK:FetchOutcome::INVALID_RESPONSE));}
 for(const char *bad:{"invalid",R"({"response":{}})",R"({"response":{"state":0,"value":"123","date":1794080000000}})",R"({"response":{"state":0,"value":123,"date":1794080000}})",R"({"response":{"state":0,"value":601,"date":1794080000000}})"}){t.reply(bad);assert(fetch_sugarsense(config,t).outcome==FetchOutcome::INVALID_RESPONSE);}
 t.reply(R"({"response":{"state":3}})");assert(fetch_sugarsense(config,t).outcome==FetchOutcome::NO_DATA);t.reply(R"({"response":{"state":0,"value":null}})");assert(fetch_sugarsense(config,t).outcome==FetchOutcome::NO_DATA);
 t.reply("{\"response\":{\"padding\":\""+std::string(16385,'a')+"\"}}");assert(fetch_sugarsense(config,t).outcome==FetchOutcome::INVALID_RESPONSE);
 t.reply(R"({"response":{}})");assert(sugar_unpair(token,t)==FetchOutcome::OK);
 esphome::clock_setup::SugarSession session;assert(esphome::clock_setup::valid_sugar_session(session));std::memcpy(session.token,token.c_str(),44);assert(esphome::clock_setup::valid_sugar_session(session));session.version=2;assert(!esphome::clock_setup::valid_sugar_session(session));session.version=1;std::memset(session.token,'a',44);assert(!esphome::clock_setup::valid_sugar_session(session));
 std::cout<<"SugarSense pairing, fixed origin, bearer auth, mg/dL, arrows, timestamp, response bounds, errors and stored-session tests passed.\n";
}

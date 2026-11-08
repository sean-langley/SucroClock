// Protocol independently implemented from the public Sugar Sense Display API:
// https://github.com/sugarsense/sugarsense-display/blob/main/firmware/sugarsense_display/net.h
#include "sugarsense_client.h"
#include <ArduinoJson.h>
#include <cmath>
namespace esphome::clock_cgm {
namespace {
FetchOutcome decode(const Reply &reply,JsonDocument &doc){
 if(reply.status==401||reply.status==403)return FetchOutcome::AUTH;
 if(reply.status==429)return FetchOutcome::RATE_LIMIT;
 if(reply.status<=0||reply.status>=500)return FetchOutcome::NETWORK;
 if(reply.status!=200||reply.body.size()>16384||deserializeJson(doc,reply.body)||!doc["response"].is<JsonObject>())return FetchOutcome::INVALID_RESPONSE;
 return FetchOutcome::OK;
}
Reply post(const char *path,const std::string &body,const std::string &token,ProviderTransport &transport){
 Headers headers{{"Content-Type","application/json"},{"Accept","application/json"},{"User-Agent","SucroClock/1"}};
 if(!token.empty())headers.emplace_back("Authorization","Bearer "+token);
 return transport.request("POST",std::string(SUGAR_API)+path,headers,body);
}
bool identifier(const std::string &value,size_t min,size_t max){if(value.size()<min||value.size()>max)return false;for(unsigned char c:value)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;return true;}
}
bool valid_sugar_token(const std::string &token){return identifier(token,43,43);}
SugarPairResult sugar_pair_start(ProviderTransport &transport){
 SugarPairResult out;JsonDocument doc;out.outcome=decode(post("/displayPairStart","{}","",transport),doc);if(out.outcome!=FetchOutcome::OK)return out;
 auto r=doc["response"];if(!r["code"].is<const char*>()||!r["pollSecret"].is<const char*>()){out.outcome=FetchOutcome::INVALID_RESPONSE;return out;}
 out.pair.code=r["code"].as<std::string>();out.pair.secret=r["pollSecret"].as<std::string>();
 if(!identifier(out.pair.code,6,6)||!identifier(out.pair.secret,1,63)){out.outcome=FetchOutcome::INVALID_RESPONSE;out.pair={};return out;}
 // Only link to the known SugarSense origin; server text never becomes HTML.
 out.pair.url="https://app.sugarsense.io/display?c="+out.pair.code;
 int expiry=r["expiresInSeconds"]|600,poll=r["pollIntervalSeconds"]|5;
 out.pair.expires_s=std::max(1,std::min(600,expiry));out.pair.poll_s=std::max(2,std::min(60,poll));return out;
}
SugarPairResult sugar_pair_poll(const SugarPair &pair,const std::string &device,ProviderTransport &transport){
 SugarPairResult out;JsonDocument body;body["pollSecret"]=pair.secret;body["deviceId"]=device;body["model"]="SucroClock HU-058D";body["appVersion"]="1.0.0";std::string payload;serializeJson(body,payload);
 auto reply=post("/displayPairPoll",payload,"",transport);if(reply.status==404){out.outcome=FetchOutcome::SESSION;return out;}
 JsonDocument doc;out.outcome=decode(reply,doc);if(out.outcome!=FetchOutcome::OK)return out;
 auto r=doc["response"];if(!r["pending"].is<bool>()){out.outcome=FetchOutcome::INVALID_RESPONSE;return out;}out.pending=r["pending"].as<bool>();if(out.pending)return out;
 if(!r["displayToken"].is<const char*>()||!valid_sugar_token(r["displayToken"].as<std::string>())){out.outcome=FetchOutcome::INVALID_RESPONSE;return out;}
 out.token=r["displayToken"].as<std::string>();return out;
}
FetchOutcome sugar_unpair(const std::string &token,ProviderTransport &transport){if(!valid_sugar_token(token))return FetchOutcome::INVALID_RESPONSE;auto reply=post("/displayUnpair","{}",token,transport);if(reply.status==200||reply.status==401||reply.status==403)return FetchOutcome::OK;JsonDocument doc;return decode(reply,doc);}
ExtraResult fetch_sugarsense(const ExtraConfig &config,ProviderTransport &transport){
 ExtraResult out;if(!valid_sugar_token(config.secret)){out.outcome=FetchOutcome::AUTH;return out;}
 JsonDocument doc;out.outcome=decode(post("/getDisplaySnapshot",R"({"hours":0,"model":"SucroClock HU-058D","appVersion":"1.0.0"})",config.secret,transport),doc);if(out.outcome!=FetchOutcome::OK)return out;
 auto r=doc["response"];out.pull_seconds=std::max(30,std::min(900,r["pullSeconds"]|60));if(!r["state"].is<int>()){out.outcome=FetchOutcome::INVALID_RESPONSE;return out;}
 if(r["state"].as<int>()!=0||r["value"].isNull()){out.outcome=FetchOutcome::NO_DATA;return out;}
 if(!r["value"].is<double>()||!r["date"].is<int64_t>()){out.outcome=FetchOutcome::INVALID_RESPONSE;return out;}
 double value=r["value"].as<double>();int64_t ms=r["date"].as<int64_t>();
 if(!std::isfinite(value)||value<20||value>600||ms<1577836800000LL||ms>4102444800000LL){out.outcome=FetchOutcome::INVALID_RESPONSE;return out;}
 int arrow=r["arrowDirection"].is<int>()?r["arrowDirection"].as<int>():0;
 const int trends[]{0,6,5,4,3,2};out.reading={int(std::lround(value)),arrow>=1&&arrow<=5?trends[arrow]:0,ms/1000,true};return out;
}
}

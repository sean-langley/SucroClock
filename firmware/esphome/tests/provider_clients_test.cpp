#include "provider_clients.h"
#include <ArduinoJson.h>
#include <cassert>
#include <iostream>
using namespace esphome::clock_cgm;
struct Call{std::string method,url,body;Headers headers;};
struct Fake:ProviderTransport {
 std::vector<Reply> replies;std::vector<Call> calls;
 Reply request(const std::string &method,const std::string &url,const Headers &headers,const std::string &body)override{assert(calls.size()<replies.size());calls.push_back({method,url,body,headers});return replies[calls.size()-1];}
 std::string sha256(const std::string &v)override{return "sha256:"+v;}std::string sha1(const std::string &v)override{return "sha1:"+v;}
};
std::string header(const Call &call,const std::string &name){for(const auto &h:call.headers)if(h.first==name)return h.second;return "";}
const char *ID1="11111111-2222-3333-4444-555555555555";
const char *ID2="aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
const std::string LOGIN=R"json({"status":0,"data":{"authTicket":{"token":"session-token"},"user":{"id":"account-id"}}})json";
const std::string ONE=R"json({"status":0,"data":[{"patientId":"11111111-2222-3333-4444-555555555555","firstName":"One","lastName":"Person","glucoseMeasurement":{"Value":6.8,"GlucoseUnits":1,"ValueInMgPerDl":123,"TrendArrow":4,"FactoryTimestamp":"01/01/2024 12:00:00 AM"}}]})json";
const std::string TWO=R"json({"status":0,"data":[{"patientId":"11111111-2222-3333-4444-555555555555","firstName":"First","glucoseMeasurement":{"ValueInMgPerDl":111,"TrendArrow":3,"FactoryTimestamp":"01/01/2024 12:00:00 AM"}},{"patientId":"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee","firstName":"Second","glucoseMeasurement":{"ValueInMgPerDl":222,"TrendArrow":1,"FactoryTimestamp":"01/01/2024 12:01:00 AM"}}]})json";
void urls_dates(){
 std::string base;assert(normalize_nightscout_url("https://example.test:8443/nightscout/",base)&&base=="https://example.test:8443/nightscout");
 for(const char *s:{"http://example.test","https://user:pass@example.test","https://example.test/?token=x","https://example.test/#x","https://example.test/../x","https://example.test/%2f","https://example.test:0","https://example.test:65536","https://example.test:\r\nheader","https:///x"})assert(!normalize_nightscout_url(s,base));
 assert(libre_timestamp("01/01/2024 12:00:00 AM")==1704067200);
 assert(libre_timestamp("01/01/2024 12:00:00 PM")==1704110400);
 assert(libre_timestamp("02/29/2024 01:00:00 AM")>0);
 for(const char *s:{"02/29/2023 01:00:00 AM","13/01/2024 01:00:00 AM","01/01/2024 00:00:00 AM","01/01/2024 01:60:00 PM","01/01/2024 01:00:00 XX","01/01/2024 01:00:00 PM junk"})assert(libre_timestamp(s)==0);
 assert(libre_host("EU2")&&libre_host("RU")&&!libre_host("evil.example"));
}
void libre_sessions(){
 ExtraConfig config;config.address="email@example.test";config.secret="p\"ass";
 Fake t;t.replies={{200,R"json({"status":0,"data":{"redirect":true,"region":"eu2"}})json"},{200,LOGIN},{200,ONE},{200,ONE},{401,""},{200,LOGIN},{200,ONE}};
 LibreSession session;auto first=session.fetch(1,config,t);assert(first.outcome==FetchOutcome::OK&&first.reading.mg_dl==123&&first.reading.trend==3&&first.reading.measured_at==1704067200);
 assert(t.calls[0].url=="https://api.libreview.io/llu/auth/login");assert(t.calls[1].url=="https://api-eu2.libreview.io/llu/auth/login");
 assert(header(t.calls[2],"Authorization")=="Bearer session-token"&&header(t.calls[2],"account-id")=="sha256:account-id");
 assert(header(t.calls[0],"version")=="4.16.0");JsonDocument body;assert(!deserializeJson(body,t.calls[0].body));assert(body["password"].as<std::string>()=="p\"ass");
 assert(session.fetch(1,config,t).outcome==FetchOutcome::OK&&t.calls.size()==4);
 assert(session.fetch(1,config,t).outcome==FetchOutcome::SESSION&&t.calls.size()==5);
 assert(session.fetch(1,config,t).outcome==FetchOutcome::OK&&t.calls[5].method=="POST");
 Fake multi;multi.replies={{200,LOGIN},{200,TWO},{200,LOGIN},{200,TWO}};LibreSession selection;
 auto result=selection.fetch(1,config,multi);assert(result.outcome==FetchOutcome::SELECTION_REQUIRED&&!result.reading.present);assert(result.choices.find("Second")!=std::string::npos);
 config.patient=ID2;result=selection.fetch(2,config,multi);assert(result.outcome==FetchOutcome::OK&&result.reading.mg_dl==222&&result.reading.trend==6);
 Fake invalid;invalid.replies={{200,R"json({"status":0,"data":{"redirect":true,"region":"evil.example"}})json"}};LibreSession rejected;assert(rejected.fetch(1,config,invalid).outcome==FetchOutcome::INVALID_RESPONSE&&invalid.calls.size()==1);
 for(const auto &pair:std::vector<std::pair<Reply,FetchOutcome>>{{{200,R"json({"status":0,"data":{"step":{"type":"pp"}}})json"},FetchOutcome::ACTION_REQUIRED},{{200,R"json({"status":2,"data":{}})json"},FetchOutcome::AUTH},{{429,""},FetchOutcome::RATE_LIMIT},{{-1,""},FetchOutcome::NETWORK}}){Fake f;f.replies={pair.first};LibreSession d;assert(d.fetch(1,config,f).outcome==pair.second&&f.calls.size()==1);}
 Fake empty;empty.replies={{200,LOGIN},{200,R"json({"status":0,"data":[]})json"}};LibreSession e;assert(e.fetch(1,config,empty).outcome==FetchOutcome::NO_DATA);
}
void nightscout(){
 ExtraConfig c;c.address="https://example.test/prefix/";c.secret="read-only-token";
 std::string data=R"json([{"sgv":145,"date":1704067200000,"direction":"Flat"}])json";
 Fake token;token.replies={{200,data}};auto result=fetch_nightscout(c,token);
 assert(result.outcome==FetchOutcome::OK&&result.reading.mg_dl==145&&result.reading.trend==4&&result.reading.measured_at==1704067200);
 assert(token.calls[0].method=="GET"&&token.calls[0].url=="https://example.test/prefix/api/v1/entries/sgv.json?count=1");assert(header(token.calls[0],"api-secret")==c.secret&&token.calls[0].url.find(c.secret)==std::string::npos);
 c.mode="API Secret";Fake api;api.replies={{200,data}};assert(fetch_nightscout(c,api).outcome==FetchOutcome::OK);assert(header(api.calls[0],"api-secret")=="sha1:read-only-token");
 c.mode="Public";Fake pub;pub.replies={{200,data}};assert(fetch_nightscout(c,pub).outcome==FetchOutcome::OK&&header(pub.calls[0],"api-secret").empty());
 for(const auto &pair:std::vector<std::pair<Reply,FetchOutcome>>{{{401,""},FetchOutcome::AUTH},{{403,""},FetchOutcome::AUTH},{{429,""},FetchOutcome::RATE_LIMIT},{{200,"[]"},FetchOutcome::NO_DATA},{{200,"{}"},FetchOutcome::INVALID_RESPONSE},{{200,R"json([{"sgv":6.8,"date":1704067200000}])json"},FetchOutcome::INVALID_RESPONSE},{{200,R"json([{"sgv":123,"date":"invalid"}])json"},FetchOutcome::INVALID_RESPONSE}}){Fake f;f.replies={pair.first};assert(fetch_nightscout(c,f).outcome==pair.second);}
 Fake iso;iso.replies={{200,R"json([{"sgv":120,"dateString":"2024-01-01T02:00:00.000+02:00","direction":"NewUnknown"}])json"}};result=fetch_nightscout(c,iso);assert(result.outcome==FetchOutcome::OK&&result.reading.measured_at==1704067200&&result.reading.trend==0);
}
int main(){urls_dates();libre_sessions();nightscout();std::cout<<"Libre redirect/session/person/unit/timestamp and Nightscout auth/URL/reading tests passed.\n";}

#include "provider_clients.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
namespace esphome::clock_cgm {
namespace {
int64_t utc(int year,int month,int day,int hour,int minute,int second) {
  if(year<2020||year>2100||month<1||month>12||hour<0||hour>23||minute<0||minute>59||second<0||second>59)return 0;
  auto leap=[](int y){return y%4==0&&(y%100!=0||y%400==0);};
  int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};if(leap(year))lengths[1]=29;
  if(day<1||day>lengths[month-1])return 0;
  int64_t days=0;for(int y=1970;y<year;++y)days+=leap(y)?366:365;
  for(int m=1;m<month;++m)days+=lengths[m-1];days+=day-1;
  return days*86400+hour*3600+minute*60+second;
}
int64_t iso_time(const std::string &s) {
  if(s.size()<20)return 0;
  for(size_t i=0;i<19;++i){if(i==4||i==7){if(s[i]!='-')return 0;}else if(i==10){if(s[i]!='T')return 0;}else if(i==13||i==16){if(s[i]!=':')return 0;}else if(s[i]<'0'||s[i]>'9')return 0;}
  int y,m,d,h,mi,se,n=0;
  if(std::sscanf(s.c_str(),"%4d-%2d-%2dT%2d:%2d:%2d%n",&y,&m,&d,&h,&mi,&se,&n)!=6)return 0;
  size_t at=n;
  if(at<s.size()&&s[at]=='.'){size_t first=++at;while(at<s.size()&&s[at]>='0'&&s[at]<='9')++at;if(at==first)return 0;}
  int offset=0;
  if(at<s.size()&&s[at]=='Z'&&at+1==s.size()){}
  else if(at<s.size()&&(s[at]=='+'||s[at]=='-')) {
    if(s.size()-at!=6||s[at+3]!=':'||s[at+1]<'0'||s[at+1]>'9'||s[at+2]<'0'||s[at+2]>'9'||s[at+4]<'0'||s[at+4]>'9'||s[at+5]<'0'||s[at+5]>'9')return 0;
    int oh,om,used=0;if(std::sscanf(s.c_str()+at+1,"%2d:%2d%n",&oh,&om,&used)!=2||at+1+used!=s.size()||oh>23||om>59)return 0;
    offset=(oh*60+om)*60*(s[at]=='+'?1:-1);
  }else return 0;
  int64_t epoch=utc(y,m,d,h,mi,se);return epoch?epoch-offset:0;
}
FetchOutcome response(const Reply &reply,JsonDocument &doc) {
  if(reply.status==429)return FetchOutcome::RATE_LIMIT;
  if(reply.status==401||reply.status==403)return FetchOutcome::AUTH;
  if(reply.status<0||reply.status>=500)return FetchOutcome::NETWORK;
  if(reply.status!=200||deserializeJson(doc,reply.body))return FetchOutcome::INVALID_RESPONSE;
  return FetchOutcome::OK;
}
int libre_trend(int arrow) { const int mapping[]={0,6,5,4,3,2};return arrow>=0&&arrow<=5?mapping[arrow]:0; }
}
const char *libre_host(const std::string &region) {
  if(region=="Auto")return "https://api.libreview.io";
  const char *regions[]={"US","EU","EU2","AE","AP","AU","CA","DE","FR","JP","LA","RU"};
  const char *hosts[]={"https://api-us.libreview.io","https://api-eu.libreview.io","https://api-eu2.libreview.io","https://api-ae.libreview.io","https://api-ap.libreview.io","https://api-au.libreview.io","https://api-ca.libreview.io","https://api-de.libreview.io","https://api-fr.libreview.io","https://api-jp.libreview.io","https://api-la.libreview.io","https://api.libreview.ru"};
  for(size_t i=0;i<12;++i)if(region==regions[i])return hosts[i];return nullptr;
}
int64_t libre_timestamp(const std::string &s) {
  int month,day,year,hour,minute,second,n=0;char ampm[3]{};
  if(std::sscanf(s.c_str(),"%2d/%2d/%4d %2d:%2d:%2d %2s%n",&month,&day,&year,&hour,&minute,&second,ampm,&n)!=7||size_t(n)!=s.size()||hour<1||hour>12)return 0;
  std::string part=ampm;if(part!="AM"&&part!="PM")return 0;
  return utc(year,month,day,hour%12+(part=="PM"?12:0),minute,second);
}
bool normalize_nightscout_url(const std::string &input,std::string &base) {
  if(input.size()>256||input.compare(0,8,"https://")!=0)return false;
  for(unsigned char c:input)if(c<=32||c>=127||c=='@'||c=='?'||c=='#'||c=='\\'||c=='%')return false;
  size_t slash=input.find('/',8);std::string authority=input.substr(8,slash==std::string::npos?slash:slash-8);
  if(authority.empty())return false;size_t colon=authority.find(':');std::string host=authority.substr(0,colon);
  if(host.empty()||host.front()=='.'||host.back()=='.'||host.find("..")!=std::string::npos)return false;
  for(char c:host)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'))return false;
  if(colon!=std::string::npos){std::string port=authority.substr(colon+1);if(port.empty()||port.size()>5)return false;unsigned number=0;for(char c:port){if(c<'0'||c>'9')return false;number=number*10+c-'0';}if(!number||number>65535)return false;}
  std::string path=slash==std::string::npos?"":input.substr(slash);
  for(char c:path)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='.'||c=='-'||c=='_'||c=='~'))return false;
  if(path.find("..")!=std::string::npos||path.find("//")!=std::string::npos)return false;
  base=input;while(base.size()>8&&base.back()=='/')base.pop_back();return true;
}
ExtraResult LibreSession::fetch(uint32_t generation,const ExtraConfig &config,ProviderTransport &transport) {
  ExtraResult result;
  if(generation!=generation_){generation_=generation;token_.clear();account_hash_.clear();const char *host=libre_host(config.region);host_=host?host:"";}
  if(host_.empty()||config.address.empty()||config.secret.empty()){result.outcome=FetchOutcome::AUTH;return result;}
  Headers headers={{"product","llu.android"},{"version",config.client_version},{"Content-Type","application/json"},{"Accept","application/json"}};
  if(token_.empty()) {
    bool logged_in=false;
    for(unsigned attempt=0;attempt<3;++attempt) {
      JsonDocument body,doc;body["email"]=config.address;body["password"]=config.secret;std::string payload;serializeJson(body,payload);
      auto reply=transport.request("POST",host_+"/llu/auth/login",headers,payload);result.outcome=response(reply,doc);if(result.outcome!=FetchOutcome::OK)return result;
      if(doc["data"]["redirect"]|false) {
        std::string region=doc["data"]["region"]|"";for(char &c:region)if(c>='a'&&c<='z')c-=32;
        const char *redirect=libre_host(region);if(!redirect||host_==redirect){result.outcome=FetchOutcome::INVALID_RESPONSE;return result;}host_=redirect;continue;
      }
      std::string step=doc["data"]["step"]["type"]|"";
      if(!step.empty()){result.outcome=FetchOutcome::ACTION_REQUIRED;return result;}
      int status=doc["status"]|-1;
      if(status!=0){result.outcome=status==2?FetchOutcome::AUTH:FetchOutcome::ACTION_REQUIRED;return result;}
      std::string token=doc["data"]["authTicket"]["token"]|"",id=doc["data"]["user"]["id"]|"";
      if(token.empty()||token.size()>4096||id.empty()||id.size()>256){result.outcome=FetchOutcome::INVALID_RESPONSE;return result;}
      for(unsigned char c:token)if(c<=32||c==127){result.outcome=FetchOutcome::INVALID_RESPONSE;return result;}
      token_=token;account_hash_=transport.sha256(id);logged_in=true;break;
    }
    if(!logged_in){result.outcome=FetchOutcome::INVALID_RESPONSE;return result;}
  }
  headers.emplace_back("Authorization","Bearer "+token_);headers.emplace_back("account-id",account_hash_);
  JsonDocument doc;auto reply=transport.request("GET",host_+"/llu/connections",headers,"");result.outcome=response(reply,doc);
  if(result.outcome==FetchOutcome::AUTH){token_.clear();result.outcome=FetchOutcome::SESSION;return result;}
  if(result.outcome!=FetchOutcome::OK)return result;
  if((doc["status"]|-1)!=0||!doc["data"].is<JsonArray>()){result.outcome=FetchOutcome::INVALID_RESPONSE;return result;}
  JsonArray connections=doc["data"].as<JsonArray>();
  if(connections.size()==0){result.outcome=FetchOutcome::NO_DATA;return result;}
  JsonDocument choices;auto list=choices.to<JsonArray>();JsonObject selected;
  for(JsonObject person:connections) {
    std::string id=person["patientId"]|"";
    if(!valid_uuid(id))continue;
    if(list.size()<8){auto entry=list.add<JsonObject>();entry["id"]=id;std::string name=std::string(person["firstName"]|"")+" "+std::string(person["lastName"]|"");entry["name"]=name.substr(0,80);if(measureJson(choices)>1800)list.remove(list.size()-1);}
    if((config.patient.empty()&&connections.size()==1)||config.patient==id)selected=person;
  }
  result.choices.clear();serializeJson(choices,result.choices);
  if(selected.isNull()){result.outcome=FetchOutcome::SELECTION_REQUIRED;return result;}
  auto measurement=selected["glucoseMeasurement"];
  if(measurement.isNull()){result.outcome=FetchOutcome::NO_DATA;return result;}
  if(!measurement["ValueInMgPerDl"].is<double>()||!measurement["FactoryTimestamp"].is<const char *>()){result.outcome=FetchOutcome::INVALID_RESPONSE;return result;}
  double value=measurement["ValueInMgPerDl"].as<double>();
  if(!std::isfinite(value)||value<20||value>600){result.outcome=FetchOutcome::INVALID_RESPONSE;return result;}
  result.reading={int(std::lround(value)),libre_trend(measurement["TrendArrow"]|0),libre_timestamp(measurement["FactoryTimestamp"].as<std::string>()),true};
  if(!result.reading.measured_at){result.outcome=FetchOutcome::INVALID_RESPONSE;result.reading={};return result;}
  result.outcome=FetchOutcome::OK;return result;
}
ExtraResult fetch_nightscout(const ExtraConfig &config,ProviderTransport &transport) {
  ExtraResult result;std::string base;
  if(!normalize_nightscout_url(config.address,base)){result.outcome=FetchOutcome::INVALID_RESPONSE;return result;}
  Headers headers={{"Accept","application/json"}};
  if(config.mode=="Token"){if(config.secret.empty()){result.outcome=FetchOutcome::AUTH;return result;}headers.emplace_back("api-secret",config.secret);}
  else if(config.mode=="API Secret"){if(config.secret.empty()){result.outcome=FetchOutcome::AUTH;return result;}headers.emplace_back("api-secret",transport.sha1(config.secret));}
  else if(config.mode!="Public"){result.outcome=FetchOutcome::AUTH;return result;}
  JsonDocument doc;auto reply=transport.request("GET",base+"/api/v1/entries/sgv.json?count=1",headers,"");result.outcome=response(reply,doc);
  if(result.outcome!=FetchOutcome::OK)return result;
  if(!doc.is<JsonArray>()){result.outcome=FetchOutcome::INVALID_RESPONSE;return result;}
  if(doc.size()==0){result.outcome=FetchOutcome::NO_DATA;return result;}
  for(JsonObject item:doc.as<JsonArray>()) {
    if(!item["sgv"].is<double>())continue;double value=item["sgv"].as<double>();int64_t measured=0;
    if(item["date"].is<double>()){double ms=item["date"].as<double>();if(std::isfinite(ms)&&ms>=1577836800000.0&&ms<=4102444800000.0&&std::floor(ms)==ms)measured=int64_t(ms)/1000;}
    else if(item["date"].isNull()&&item["dateString"].is<const char *>())measured=iso_time(item["dateString"].as<std::string>());
    if(!measured||!std::isfinite(value)||value<20||value>600)continue;
    if(!result.reading.present||measured>result.reading.measured_at)result.reading={int(std::lround(value)),trend_from_name(item["direction"]|""),measured,true};
  }
  result.outcome=result.reading.present?FetchOutcome::OK:FetchOutcome::INVALID_RESPONSE;return result;
}
}  // namespace

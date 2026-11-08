#include "dexcom_client.h"
#include <ArduinoJson.h>
#include <cassert>
#include <iostream>
#include <vector>
using namespace esphome::clock_cgm;
const std::string ACCOUNT = "11111111-2222-3333-4444-555555555555";
const std::string SESSION = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
const std::string SAMPLE = R"json([{"Value":123,"Trend":"FortyFiveUp","DT":"Date(1794092400000-0700)"}])json";
struct Fake : DexcomTransport {
  std::vector<Reply> replies;
  std::vector<std::string> paths, payloads;
  std::vector<Region> regions;
  Reply post(Region region, const std::string &path, const std::string &payload) override {
    assert(paths.size() < replies.size());
    paths.push_back(path); payloads.push_back(payload); regions.push_back(region);
    return replies[paths.size() - 1];
  }
};
Reply token(const std::string &s) { return {200, "\"" + s + "\""}; }
void parsing_and_freshness() {
  assert(parse_measurement_time("Date(1794092400000-0700)") == 1794092400);
  assert(parse_measurement_time("/Date(1794092400000+0000)/") == 1794092400);
  assert(parse_measurement_time("Date(1794092400000)") == 1794092400);
  for (const char *s : {"", "1794092400000", "Date(-1)", "Date(123)", "Date(1794092400000000000000)", "Date(1794092400000+abc0)", "Date(1794092400000)junk"})
    assert(parse_measurement_time(s) == 0);
  Reading r{123, 3, 1794092400, true};
  assert(fresh(r,1794092400));
  assert(fresh(r,1794093300));
  assert(!fresh(r,1794093301));
  assert(!fresh(r,1794092339));  // Implausibly future sample.
  assert(!fresh(r,0));
  r.present = false; assert(!fresh(r,1794092400));
  r.present = true; r.mg_dl = 0; assert(!fresh(r,1794092400));
  assert(trend_glyph(0).empty()); assert(trend_glyph(8).empty()); assert(trend_glyph(4) == "-");
  assert(trend_glyph(1) == ">"); assert(trend_glyph(7) == "<");
  assert(trend_from_name("FortyFiveUp") == 3); assert(trend_from_name("not-a-trend") == 0);
  assert(!valid_uuid("00000000-0000-0000-0000-000000000000")); assert(valid_uuid(SESSION));
  assert(retry_delay(1)==60000); assert(retry_delay(2)==120000); assert(retry_delay(100)==960000);
}
void session_and_region() {
  Fake t;
  t.replies = {token(ACCOUNT),token(SESSION),{200,SAMPLE},{200,SAMPLE},
    {500,R"json({"Code":"SessionNotValid"})json"}, token(SESSION),{200,SAMPLE},
    token(ACCOUNT),token(SESSION),{200,SAMPLE}};
  DexcomSession d;
  auto a=d.fetch(1,Region::US,"user\"name","pass\\word",t);
  assert(a.outcome == FetchOutcome::OK && a.reading.mg_dl==123 && a.reading.trend==3);
  assert(a.reading.measured_at == 1794092400);
  assert(t.paths[0] == "General/AuthenticatePublisherAccount");
  assert(t.paths[1] == "General/LoginPublisherAccountById");
  JsonDocument j; assert(!deserializeJson(j,t.payloads[0]));
  assert(j["accountName"].as<std::string>()=="user\"name");
  assert(j["password"].as<std::string>()=="pass\\word");
  assert(j["applicationId"].as<std::string>()==application_id(Region::US));
  auto b=d.fetch(1,Region::US,"user\"name","pass\\word",t);
  assert(b.outcome==FetchOutcome::OK && t.paths.size()==4); // No repeated login.
  assert(t.paths[3].find("sessionId="+SESSION)!=std::string::npos);
  auto c=d.fetch(1,Region::US,"user\"name","pass\\word",t);
  assert(c.outcome==FetchOutcome::SESSION && t.paths.size()==5);
  auto e=d.fetch(1,Region::US,"user\"name","pass\\word",t);
  assert(e.outcome==FetchOutcome::OK && t.paths[5]=="General/LoginPublisherAccountById");
  auto f=d.fetch(2,Region::JP,"user\"name","pass\\word",t);
  assert(f.outcome==FetchOutcome::OK && t.paths[7]=="General/AuthenticatePublisherAccount");
  assert(t.regions[7]==Region::JP);
  assert(!deserializeJson(j,t.payloads[7])); assert(j["applicationId"].as<std::string>()==application_id(Region::JP));
  assert(std::string(base_url(Region::US)).find("share2.dexcom.com")!=std::string::npos);
  assert(std::string(base_url(Region::OUS)).find("shareous1.dexcom.com")!=std::string::npos);
}
void failure_cases() {
  auto login_failure=[](Reply reply,FetchOutcome expected) {
    DexcomSession d;Fake t;t.replies={reply};
    assert(d.fetch(1,Region::US,"u","p",t).outcome==expected);assert(t.paths.size()==1);
  };
  login_failure({500,R"json({"Code":"AccountPasswordInvalid"})json"},FetchOutcome::AUTH);
  login_failure({500,R"json({"Code":"SSO_AuthenticateMaxAttemptsExceeded"})json"},FetchOutcome::AUTH);
  login_failure({429,""},FetchOutcome::RATE_LIMIT);
  login_failure({-1,""},FetchOutcome::NETWORK);
  login_failure({200,"not JSON"},FetchOutcome::INVALID_RESPONSE);
  login_failure(token("00000000-0000-0000-0000-000000000000"),FetchOutcome::INVALID_RESPONSE);
  login_failure({401,"Unauthorized"},FetchOutcome::AUTH);
  for (const auto &pair : std::vector<std::pair<std::string,FetchOutcome>>{
      {"[]",FetchOutcome::NO_DATA}, {"{}",FetchOutcome::INVALID_RESPONSE},
      {R"json([{"Value":0,"DT":"Date(1794092400000)"}])json",FetchOutcome::INVALID_RESPONSE},
      {R"json([{"Value":123,"DT":"bad"}])json",FetchOutcome::INVALID_RESPONSE},
      {R"json([{"Value":"123","DT":"Date(1794092400000)"}])json",FetchOutcome::INVALID_RESPONSE},
      {R"json([{"Value":123,"Trend":"UnknownFutureTrend","DT":"Date(1794092400000)"}])json",FetchOutcome::OK}}) {
    DexcomSession d; Fake t; t.replies={token(ACCOUNT),token(SESSION),{200,pair.first}};
    auto result=d.fetch(1,Region::US,"u","p",t);assert(result.outcome==pair.second);
    if(pair.second==FetchOutcome::OK) assert(result.reading.trend==0);
  }
}
int main() { parsing_and_freshness(); session_and_region(); failure_cases(); std::cout << "Dexcom parsing, freshness, session reuse, region reset and error tests passed.\n"; }

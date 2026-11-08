#include "dexcom_client.h"
#include <ArduinoJson.h>

namespace esphome::clock_cgm {
namespace {
std::string encode(JsonDocument &doc) {
  std::string body;
  serializeJson(doc, body);
  return body;
}
}
FetchResult DexcomSession::fetch(uint32_t generation, Region region, const std::string &username,
                               const std::string &password, DexcomTransport &transport) {
  if (generation != generation_) {
    account_id_.clear(); session_id_.clear(); generation_ = generation;
  }
  FetchResult result;
    auto decode = [&](const Reply &reply, JsonDocument &doc, bool login) -> bool {
      if (reply.status == 429) { result.outcome = FetchOutcome::RATE_LIMIT; return false; }
      if (reply.status < 0) return false;
      if (reply.status == 401 || (login && reply.status == 403)) {
        if (!login) session_id_.clear();
        result.outcome = login ? FetchOutcome::AUTH : FetchOutcome::SESSION;
        return false;
      }
      if (deserializeJson(doc, reply.body)) { result.outcome = FetchOutcome::INVALID_RESPONSE; return false; }
      const char *code = doc["Code"] | "";
      std::string error(code);
      if (error == "SessionIdNotFound" || error == "SessionNotValid" || (!login && reply.status == 401)) {
        session_id_.clear(); result.outcome = FetchOutcome::SESSION; return false;
      }
      if (reply.status == 401 || error.compare(0, 7, "Account") == 0 || error.compare(0, 4, "SSO_") == 0 || error == "InvalidArgument") {
        result.outcome = FetchOutcome::AUTH; return false;
      }
      if (reply.status != 200 || !error.empty()) return false;
      return true;
    };
    bool ready = true;
    if (account_id_.empty()) {
      JsonDocument body, reply;
      body["accountName"] = username;
      body["password"] = password;
      body["applicationId"] = application_id(region);
      ready = decode(transport.post(region, "General/AuthenticatePublisherAccount", encode(body)), reply, true);
      if (ready && reply.is<const char *>()) account_id_ = reply.as<std::string>();
      if (ready && !valid_uuid(account_id_)) { ready = false; result.outcome = FetchOutcome::INVALID_RESPONSE; }
    }
    if (ready && session_id_.empty()) {
      JsonDocument body, reply;
      body["accountId"] = account_id_;
      body["password"] = password;
      body["applicationId"] = application_id(region);
      ready = decode(transport.post(region, "General/LoginPublisherAccountById", encode(body)), reply, true);
      if (ready && reply.is<const char *>()) session_id_ = reply.as<std::string>();
      if (ready && !valid_uuid(session_id_)) { ready = false; result.outcome = FetchOutcome::INVALID_RESPONSE; }
    }
    if (ready) {
      JsonDocument reply;
      ready = decode(transport.post(region, "Publisher/ReadPublisherLatestGlucoseValues?sessionId=" + session_id_ + "&minutes=1440&maxCount=1", "{}"), reply, false);
      if (ready) {
        if (reply.is<JsonArray>() && reply.size() == 0) result.outcome = FetchOutcome::NO_DATA;
        else if (reply.is<JsonArray>() && reply.size() == 1 && reply[0]["Value"].is<int>() && reply[0]["DT"].is<const char *>()) {
          auto item = reply[0];
          Reading reading;
          reading.mg_dl = item["Value"].as<int>();
          reading.measured_at = parse_measurement_time(item["DT"].as<std::string>());
          reading.trend = item["Trend"].is<const char *>() ? trend_from_name(item["Trend"].as<std::string>()) : item["Trend"].is<int>() ? item["Trend"].as<int>() : 0;
          if (reading.trend < 0 || reading.trend > 9) reading.trend = 0;
          reading.present = reading.measured_at > 0 && reading.mg_dl >= 20 && reading.mg_dl <= 600;
          if (reading.present) { result.reading = reading; result.outcome = FetchOutcome::OK; }
          else result.outcome = FetchOutcome::INVALID_RESPONSE;
        } else result.outcome = FetchOutcome::INVALID_RESPONSE;
      }
    }
  return result;
}
}  // namespace esphome::clock_cgm

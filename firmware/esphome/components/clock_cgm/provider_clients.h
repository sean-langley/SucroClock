#pragma once
#include "dexcom_client.h"
#include <vector>
#include <utility>
namespace esphome::clock_cgm {
using Headers = std::vector<std::pair<std::string,std::string>>;
class ProviderTransport {
 public:
  virtual ~ProviderTransport() = default;
  virtual Reply request(const std::string &method,const std::string &url,const Headers &headers,const std::string &body) = 0;
  virtual std::string sha256(const std::string &value) = 0;
  virtual std::string sha1(const std::string &value) = 0;
};
struct ExtraConfig {
  std::string address,secret,region{"Auto"},patient,mode{"Token"},client_version{"4.16.0"};
};
struct ExtraResult { FetchOutcome outcome{FetchOutcome::NETWORK};Reading reading;std::string choices{"[]"};uint32_t pull_seconds{60}; };
bool normalize_nightscout_url(const std::string &input,std::string &base);
const char *libre_host(const std::string &region);
int64_t libre_timestamp(const std::string &value);
class LibreSession {
 public:
  ExtraResult fetch(uint32_t generation,const ExtraConfig &config,ProviderTransport &transport);
 protected:
  uint32_t generation_{0};
  std::string host_,token_,account_hash_;
};
ExtraResult fetch_nightscout(const ExtraConfig &config,ProviderTransport &transport);
}  // namespace

#pragma once
#include "provider_clients.h"
namespace esphome::clock_cgm {
bool init_provider_lock();
class ProviderWorkGuard {
 public:
  ProviderWorkGuard();~ProviderWorkGuard();
  bool locked() const {return locked_;}
 protected: bool locked_{false};
};
Reply https_request(const std::string &method,const std::string &url,const Headers &headers,const std::string &body,size_t limit=16384);
class SecureProviderTransport : public ProviderTransport {
 public:
  Reply request(const std::string &method,const std::string &url,const Headers &headers,const std::string &body) override {return https_request(method,url,headers,body);}
  std::string sha256(const std::string &value) override;
  std::string sha1(const std::string &value) override;
};
}

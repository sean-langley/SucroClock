#pragma once
#include "esphome/components/clock_cgm/sugarsense_client.h"
#include <cstdint>
#include <cstring>
#include <string>
#include "esphome/components/clock_cgm/provider_clients.h"
namespace esphome::clock_setup {
struct SugarSession {uint32_t version{1};char token[44]{};};
inline bool valid_sugar_session(const SugarSession &record){return record.version==1&&std::memchr(record.token,0,sizeof(record.token))&&(record.token[0]==0||clock_cgm::valid_sugar_token(record.token));}
constexpr size_t MAX_CREDENTIAL = 256;
struct Credentials {
  uint32_t version{1};
  char username[MAX_CREDENTIAL + 1]{};
  char password[MAX_CREDENTIAL + 1]{};
};
inline bool valid_field(const std::string &value) {
  if (value.size() > MAX_CREDENTIAL) return false;
  for (unsigned char c : value) if (c < 32 || c == 127) return false;
  return true;
}
inline bool valid_record(const Credentials &record) {
  return record.version == 1 && std::memchr(record.username, 0, sizeof(record.username)) &&
         std::memchr(record.password, 0, sizeof(record.password)) &&
         valid_field(record.username) && valid_field(record.password);
}
inline bool merge_credentials(const Credentials &old, const std::string &username,
                              const std::string &password, bool forget, Credentials &result) {
  if (forget) { result = {}; return true; }
  if (!valid_field(username) || !valid_field(password) || username.empty()) return false;
  result = old;
  result.version = 1;
  std::memset(result.username, 0, sizeof(result.username));
  std::memcpy(result.username, username.data(), username.size());
  if (!password.empty()) {
    std::memset(result.password, 0, sizeof(result.password));
    std::memcpy(result.password, password.data(), password.size());
  }
  // Keeping an existing password is explicit through a blank password field.
  return result.password[0] != 0;
}
struct ProviderSettings {
  uint32_t version{1};
  char address[257]{};
  char secret[257]{};
  char region[16]{"Auto"};
  char patient[65]{};
  char mode[16]{"Token"};
  char client_version[16]{"4.16.0"};
};
inline bool terminated(const char *p,size_t n){return std::memchr(p,0,n)!=nullptr;}
inline bool valid_provider_record(const ProviderSettings &r,bool libre){
  if(r.version!=1||!terminated(r.address,sizeof(r.address))||!terminated(r.secret,sizeof(r.secret))||
     !terminated(r.region,sizeof(r.region))||!terminated(r.patient,sizeof(r.patient))||!terminated(r.mode,sizeof(r.mode))||!terminated(r.client_version,sizeof(r.client_version)))return false;
  if(!valid_field(r.address)||!valid_field(r.secret)||!valid_field(r.region)||!valid_field(r.patient)||!valid_field(r.mode)||!valid_field(r.client_version))return false;
  if(libre){
    if(!clock_cgm::libre_host(r.region)||(!std::string(r.patient).empty()&&!clock_cgm::valid_uuid(r.patient)))return false;
    std::string version=r.client_version;if(version.empty()||version.size()>15||version.front()=='.'||version.back()=='.'||version.find("..")!=std::string::npos)return false;
    for(char c:version)if((c<'0'||c>'9')&&c!='.')return false;
    return true;
  }
  std::string normalized;return (r.address[0]==0||clock_cgm::normalize_nightscout_url(r.address,normalized))&&
     (std::string(r.mode)=="Token"||std::string(r.mode)=="API Secret"||std::string(r.mode)=="Public");
}
inline clock_cgm::ExtraConfig extra_config(const ProviderSettings &r){return {r.address,r.secret,r.region,r.patient,r.mode,r.client_version};}
inline bool merge_provider(const ProviderSettings &old,const ProviderSettings &input,bool libre,bool forget,ProviderSettings &out){
  if(forget){out={};return true;}
  if(!valid_provider_record(input,libre)||!input.address[0])return false;
  out=input;
  if(!input.secret[0])std::memcpy(out.secret,old.secret,sizeof(out.secret));
  return (!libre && std::string(out.mode)=="Public") || out.secret[0]!=0;
}
inline bool constant_equal(const std::string &a, const std::string &b) {
  if (a.size() != b.size()) return false;
  unsigned char difference = 0;
  for (size_t i = 0; i < a.size(); ++i) difference |= static_cast<unsigned char>(a[i] ^ b[i]);
  return difference == 0;
}
}  // namespace esphome::clock_setup

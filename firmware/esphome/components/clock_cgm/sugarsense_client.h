#pragma once
#include "provider_clients.h"
namespace esphome::clock_cgm {
constexpr const char *SUGAR_API="https://app.sugarsense.io/api/v1";
struct SugarPair {std::string code,secret,url;uint32_t expires_s{600},poll_s{5};};
struct SugarPairResult {FetchOutcome outcome{FetchOutcome::NETWORK};SugarPair pair;bool pending{false};std::string token;};
bool valid_sugar_token(const std::string &token);
SugarPairResult sugar_pair_start(ProviderTransport &transport);
SugarPairResult sugar_pair_poll(const SugarPair &pair,const std::string &device,ProviderTransport &transport);
FetchOutcome sugar_unpair(const std::string &token,ProviderTransport &transport);
ExtraResult fetch_sugarsense(const ExtraConfig &config,ProviderTransport &transport);
}

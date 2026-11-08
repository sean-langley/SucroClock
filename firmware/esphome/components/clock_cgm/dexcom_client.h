#pragma once
#include "reading.h"

namespace esphome::clock_cgm {
enum class FetchOutcome { OK, NO_DATA, NETWORK, INVALID_RESPONSE, SESSION, AUTH, RATE_LIMIT, ACTION_REQUIRED, SELECTION_REQUIRED };
struct FetchResult { FetchOutcome outcome{FetchOutcome::NETWORK}; Reading reading; };
struct Reply { int status{-1}; std::string body; };
class DexcomTransport {
 public:
  virtual ~DexcomTransport() = default;
  virtual Reply post(Region region, const std::string &path, const std::string &payload) = 0;
};
class DexcomSession {
 public:
  FetchResult fetch(uint32_t generation, Region region, const std::string &username,
                    const std::string &password, DexcomTransport &transport);
 protected:
  uint32_t generation_{0};
  std::string account_id_, session_id_;
};
}  // namespace esphome::clock_cgm

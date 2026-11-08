#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>

namespace esphome::clock_cgm {
enum class Region : uint8_t { US, OUS, JP };
struct Reading {
  int mg_dl{0};
  int trend{0};
  int64_t measured_at{0};  // Measurement time, UTC seconds, never the fetch time.
  bool present{false};
};
inline const char *base_url(Region region) {
  switch (region) {
    case Region::US: return "https://share2.dexcom.com/ShareWebServices/Services/";
    case Region::OUS: return "https://shareous1.dexcom.com/ShareWebServices/Services/";
    case Region::JP: return "https://share.dexcom.jp/ShareWebServices/Services/";
  }
  return nullptr;
}
inline const char *application_id(Region region) {
  return region == Region::JP ? "d8665ade-9673-4e27-9ff6-92db4ce13d13" : "d89443d2-327c-4a6f-89e5-496bbb0317db";
}
inline bool valid_uuid(const std::string &s) {
  if (s.size() != 36 || s == "00000000-0000-0000-0000-000000000000") return false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) { if (s[i] != '-') return false; }
    else if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f') || (s[i] >= 'A' && s[i] <= 'F'))) return false;
  }
  return true;
}
// Accept Dexcom's Date(ms+offset) and /Date(ms)/ envelopes. The milliseconds
// already represent a UTC instant: the optional offset must not be added again.
inline int64_t parse_measurement_time(const std::string &s) {
  size_t start = s.compare(0, 6, "/Date(") == 0 ? 6 : s.compare(0, 5, "Date(") == 0 ? 5 : std::string::npos;
  if (start == std::string::npos) return 0;
  size_t i = start;
  int64_t ms = 0;
  while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
    if (i - start >= 13) return 0;
    ms = ms * 10 + (s[i++] - '0');
  }
  if (i == start) return 0;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
    if (i + 5 >= s.size()) return 0;
    for (size_t j = i + 1; j <= i + 4; ++j) if (s[j] < '0' || s[j] > '9') return 0;
    i += 5;
  }
  if (s.substr(i) != (start == 6 ? ")/" : ")")) return 0;
  return ms >= 1577836800000LL && ms <= 4102444800000LL ? ms / 1000 : 0;
}
inline int trend_from_name(const std::string &s) {
  const char *names[] = {"None", "DoubleUp", "SingleUp", "FortyFiveUp", "Flat", "FortyFiveDown", "SingleDown", "DoubleDown", "NotComputable", "RateOutOfRange"};
  for (int i = 0; i < 10; ++i) if (s == names[i]) return i;
  return 0;
}
inline const char *trend_name(int trend) {
  const char *names[] = {"Unknown", "Double up", "Up", "Slightly up", "Flat", "Slightly down", "Down", "Double down", "Not computable", "Rate out of range"};
  return names[trend >= 0 && trend <= 9 ? trend : 0];
}
inline std::string trend_glyph(int trend) {
  if (trend == 1) return ">";
  if (trend == 2 || trend == 3) return "^";
  if (trend == 4) return "-";
  if (trend == 5 || trend == 6) return "_";
  if (trend == 7) return "<";
  return "";  // Unknown trend must never be presented as flat.
}
inline bool fresh(const Reading &r, int64_t now, int64_t max_age = 900) {
  return r.present && r.mg_dl >= 20 && r.mg_dl <= 600 && r.measured_at > 0 &&
         now >= 1577836800 && r.measured_at <= now + 60 && now - r.measured_at <= max_age;
}
inline uint32_t retry_delay(unsigned failures) {
  unsigned shift = failures > 5 ? 4 : failures == 0 ? 0 : failures - 1;
  return 60000U << shift;  // 1, 2, 4, 8, 16 minutes; no rapid login loop.
}
}  // namespace esphome::clock_cgm

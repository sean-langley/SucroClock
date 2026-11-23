#pragma once
#include <array>
namespace esphome::aip33628 {
// Providers retain integer mg/dL internally. Rounding to mmol/L tenths is
// exactly round(mg/dL / 18 * 10), with no binary floating-point edge cases.
constexpr int glucose_mmol_tenths(int mg_dl) { return (mg_dl * 5 + 4) / 9; }
struct GlucoseDigits {
  std::array<char, 4> digits{{' ', ' ', ' ', '-'}};
  bool lower_decimal{false};
};
inline GlucoseDigits mmol_digits(int mg_dl, char trend) {
  GlucoseDigits out;
  if (mg_dl < 20 || mg_dl > 600) return out;
  const int tenths = glucose_mmol_tenths(mg_dl);
  const int whole = tenths / 10;
  out.digits = {{whole >= 10 ? char('0' + whole / 10) : ' ',
                 char('0' + whole % 10), char('0' + tenths % 10), trend}};
  out.lower_decimal = true;
  return out;
}
}  // namespace esphome::aip33628

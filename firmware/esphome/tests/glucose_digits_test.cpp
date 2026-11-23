#include "esphome/components/aip33628/glucose_digits.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
using namespace esphome::aip33628;
int main() {
  assert(std::string(mmol_digits(108, '-').digits.data(), 4) == " 60-");
  assert(std::string(mmol_digits(221, '^').digits.data(), 4) == "123^");
  assert(std::string(mmol_digits(70, '_').digits.data(), 4) == " 39_");
  assert(std::string(mmol_digits(180, '-').digits.data(), 4) == "100-");
  assert(std::string(mmol_digits(600, '\0').digits.data(), 3) == "333");
  for (int mg = 20; mg <= 600; ++mg) {
    const auto frame = mmol_digits(mg, '^');
    const int expected = int(std::lround(mg / 18.0 * 10));
    const int tens = frame.digits[0] == ' ' ? 0 : frame.digits[0] - '0';
    const int actual = tens * 100 + (frame.digits[1] - '0') * 10 + frame.digits[2] - '0';
    assert(actual == expected && frame.lower_decimal && frame.digits[3] == '^');
  }
  for (int invalid : {0, 19, 601, -1}) {
    auto frame = mmol_digits(invalid, '^');
    assert(!frame.lower_decimal && std::string(frame.digits.data(), 4) == "   -");
  }
  std::cout << "mmol/L conversion, decimal layout, trend retention and invalid-value tests passed.\n";
}

#pragma once

namespace meta {
consteval float factorial(unsigned n) noexcept {
  return n ? n * factorial(n - 1) : 1.0;
}

consteval float pow(float x, unsigned n) noexcept {
  if (!n) {
    return 1.0;
  } else if (n % 2) {
    return x * pow(x * x, (n - 1) / 2);
  } else {
    return pow(x * x, n / 2);
  }
}

consteval float cos(float x) noexcept {
  float result = 0.F;
  for (unsigned i{}; i < 16; ++i) {
    auto const sign = (i % 2) ? -1.F : 1.F;
    result += sign * pow(x, i * 2) / factorial(i * 2);
  }

  return result;
}

consteval float sin(float x) noexcept {
  float result = 0.F;
  for (unsigned i{}; i < 16; ++i) {
    auto const sign = (i % 2) ? -1.F : 1.F;
    result += sign * pow(x, 1 + i * 2) / factorial(1 + i * 2);
  }

  return result;
}

consteval unsigned log2(unsigned n) noexcept {
  unsigned result{};
  for (; n > 1U; n >>= 1U) {
    ++result;
  }

  return result;
}
} // namespace meta

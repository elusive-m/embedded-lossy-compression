#pragma once
#include <fmt/format.h>
#include <string_view>

#if __cplusplus >= 201703L
#define register
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include <Arduino.h>
#pragma GCC diagnostic pop

#undef true
#undef false
#undef min
#undef max
#undef abs

template <typename... Args>
  requires(sizeof...(Args) > 0)
void serial(fmt::format_string<Args...> format, Args &&...args) {
  constexpr auto BUFFER_SIZE{128U};

  std::array<char, BUFFER_SIZE> buffer{};
  const auto [_, size] = fmt::format_to_n(buffer.begin(), BUFFER_SIZE, format,
                                          std::forward<Args>(args)...);

  Serial.write(buffer.data(), size);
}

void serial(std::string_view s) { Serial.write(s.data(), s.size()); }

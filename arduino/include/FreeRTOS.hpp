#pragma once
#include <algorithm>
#include <array>
#include <functional>
#include <type_traits>

#include <FreeRTOS.h>
#include <task.h>

namespace meta {
using nullary_fn_ptr = std::add_pointer_t<void()>;
template <typename F>
concept nullary_invocable = std::is_invocable_r_v<void, F>;
template <typename F>
concept stateless_nullary_invocable =
    nullary_invocable<F> && std::convertible_to<F, nullary_fn_ptr>;

template <auto N> struct string {
  static_assert(N <= configMAX_TASK_NAME_LEN);

  char buffer[N]{};
  consteval string(const char (&s)[N]) noexcept {
    std::ranges::copy(s, buffer);
  }
};

template <auto N> string(const char (&)[N]) -> string<N>;
} // namespace meta

using priority_t = UBaseType_t;
using stack_size_t = configSTACK_DEPTH_TYPE;

struct task_parameters {
  priority_t priority;
  stack_size_t stack_size{256};
};

template <meta::string name>
std::optional<TaskHandle_t>
task(task_parameters parameters,
     meta::stateless_nullary_invocable auto f) noexcept {
  TaskHandle_t handle{};
  const auto status = xTaskCreate(
      +[](void *parameter) {
        reinterpret_cast<meta::nullary_fn_ptr>(parameter)();
      },
      name.buffer, parameters.stack_size,
      reinterpret_cast<void *>(static_cast<meta::nullary_fn_ptr>(f)),
      parameters.priority, &handle);

  return (status == pdPASS) ? std::make_optional(handle) : std::nullopt;
}

template <meta::string name>
std::optional<TaskHandle_t> task(task_parameters parameters,
                                 meta::nullary_invocable auto f) noexcept {
  using proxy = std::function<void()>;

  TaskHandle_t handle{};
  const auto status = xTaskCreate(
      +[](void *parameter) {
        const auto *p = static_cast<proxy *>(parameter);
        const auto f = std::move(*p);

        // Omit empty function call check
        if (!f) {
          __builtin_unreachable();
        }

        delete p;
        f();
      },
      name.buffer, parameters.stack_size,
      new (std::nothrow) proxy(std::move(f)), parameters.priority, &handle);

  return (status == pdPASS) ? std::make_optional(handle) : std::nullopt;
}

void sleep(TickType_t ticks) noexcept { vTaskDelay(ticks); }

consteval auto operator""_ms(unsigned long long ms) noexcept {
  return pdMS_TO_TICKS(ms);
}

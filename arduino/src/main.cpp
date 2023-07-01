#include "config.hpp"
#include <bit>
#include <complex>
#include <double_buffer.hpp>
#include <limits>
#include <ranges>
#include <rfft.hpp>

#define RX Serial
#define TX Serial
// #define TX SerialUSB

static_assert((WINDOW_SIZE & (WINDOW_SIZE - 1)) == 0,
              "BATCH_SIZE must be a power of two");

// Comparison predicate that computes |a| + |b| instead of a^2 + b^2
bool farthest_from_origin(complex const &a, complex const &b) noexcept {
  constexpr auto flatten = [](auto const &x) {
    return std::abs(x.real()) + std::abs(x.imag());
  };

  return flatten(a) < flatten(b);
}

// Omits the square root
float abs2(complex const &x) noexcept {
  return (x.real() * x.real()) + (x.imag() * x.imag());
}

template <typename T> void transmit(T const &value) noexcept {
  TX.write(reinterpret_cast<std::uint8_t *>(&value), sizeof(T));
}

template <auto N>
void compress_and_transmit(std::array<complex, N> const &data) noexcept {
  static constexpr auto THRESHOLD2 = AMPLITUDE_THRESHOLD * AMPLITUDE_THRESHOLD;

  auto const peak = std::ranges::max_element(data, farthest_from_origin);
  auto const peak_magnitude = abs2(*peak);
  if (peak_magnitude == 0.F) {
    transmit(PACKET_END);
    return;
  }

  auto const multiplier = 1.F / peak_magnitude;
  for (unsigned i{}; i < N; ++i) {
    auto const &c = data[i];

    if ((abs2(c) * multiplier) >= THRESHOLD2) {
      transmit(i);
      transmit(c);
    }
  }

  transmit(PACKET_END);
}

double_buffer<std::vector<complex>> shared_data;
void setup() {
  RX.begin(BAUD_RATE);
  RX.setTimeout(1);

  TX.begin(BAUD_RATE);
  TX.setTimeout(1);

  shared_data.raw_get(0).reserve(WINDOW_SIZE);
  shared_data.raw_get(1).reserve(WINDOW_SIZE);

  task<"RX">({.priority = 3}, [] {
    auto tick = xTaskGetTickCount();
    auto *buffer = &shared_data.start_writing();
    // DEBUG
    auto const *previous_buffer = buffer;

    for (;;) {
      for (auto bytes_available = static_cast<std::size_t>(RX.available());
           (bytes_available > 0U) && ((bytes_available % sizeof(float)) == 0);
           bytes_available -= sizeof(float)) {
        float result;
        RX.readBytes(reinterpret_cast<std::uint8_t *>(&result), sizeof(float));

        buffer->emplace_back(result, 0.F);
        if (buffer->size() == WINDOW_SIZE) {
          shared_data.stop_writing();

          buffer = &shared_data.start_writing();
          buffer->clear();

          if (previous_buffer == buffer) {
            // We missed our deadline
            RX.end();
          }

          previous_buffer = buffer;
        }
      }

      vTaskDelayUntil(&tick, SAMPLING_INTERVAL);
    }
  });

  task<"TX">({.priority = 2, .stack_size = 1024}, [] {
    auto tick = xTaskGetTickCount();

    for (;;) {
      if (auto *const p = shared_data.start_reading(); p != nullptr) {
        auto &samples = *p;
        auto const result = rfft<WINDOW_SIZE>(samples.data());
        shared_data.end_reading();

        compress_and_transmit(result);
      }

      vTaskDelayUntil(&tick, WINDOW_SIZE * SAMPLING_INTERVAL);
    }
  });

  while (!RX)
    ;

  vTaskStartScheduler();
}

void loop() {}

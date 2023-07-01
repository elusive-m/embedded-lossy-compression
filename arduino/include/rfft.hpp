// Adapted from: https://www.nayuki.io/page/free-small-fft-in-multiple-languages

#pragma once
#include "meta.hpp"
#include <array>
#include <complex>

using complex = std::complex<float>;
inline constexpr float pi = 3.14159265F;
template <auto I, auto N> consteval auto W() noexcept {
  return complex{meta::cos(2 * pi * I / N), meta::sin(2 * pi * I / N)};
}

struct stride_view {
  complex *data_{nullptr};

  constexpr complex &operator[](unsigned i) noexcept { return data_[i * 2U]; }
};

template <auto Width> constexpr auto reverse_bits(unsigned x) noexcept {
  unsigned result{};
  for (unsigned i{}; i < Width; ++i, x >>= 1U) {
    result = (result << 1U) | (x & 1U);
  }

  return result;
}

template <auto N> void fft_inplace(stride_view samples) noexcept {
  static constexpr auto TWIDDLE_FACTORS =
      []<auto... Is>(std::index_sequence<Is...>) {
        return std::array{W<Is, N>()...};
      }(std::make_index_sequence<N / 2>{});

  for (unsigned i{}; i < N; ++i) {
    auto const j = reverse_bits<meta::log2(N)>(i);
    if (j > i) {
      std::swap(samples[i], samples[j]);
    }
  }

  // Cooley-Tukey decimation-in-time radix-2 FFT
  for (auto size = 2U; size <= N; size *= 2U) {
    auto const half_size = size / 2U;
    auto const table_step = N / size;
    for (unsigned i{}; i < N; i += size) {
      for (unsigned j = i, k{}; j < (i + half_size); ++j, k += table_step) {
        auto &a = samples[j];
        auto &b = samples[j + half_size];

        auto const t = b * TWIDDLE_FACTORS[k];
        b = a - t;
        a += t;
      }
    }
  }
}

template <auto N> std::array<complex, N / 2 + 1> rfft(complex *input) noexcept {
  static_assert((N & (N - 1)) == 0, "N must be a power of two");
  static constexpr auto TWIDDLE_FACTORS =
      []<auto... Is>(std::index_sequence<Is...>) {
        return std::array{W<Is, N>()...};
      }(std::make_index_sequence<N / 2>{});

  std::array<complex, N / 2 + 1> result;

  auto f = stride_view{input};
  auto g = stride_view{std::next(input)};
  fft_inplace<N / 2>(f);
  fft_inplace<N / 2>(g);

  for (unsigned i{}; i < N / 2; ++i) {
    result[i] = f[i] + TWIDDLE_FACTORS[i] * g[i];
  }

  result[N / 2] = f[0] - g[0];
  return result;
}

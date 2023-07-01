#include <Due.hpp>
#include <FreeRTOS.hpp>
#include <complex>

using complex = std::complex<float>;

inline constexpr auto BAUD_RATE = 115_200U;
inline constexpr auto WINDOW_SIZE = 64;
inline constexpr auto SAMPLING_INTERVAL = 1_ms;
inline constexpr auto AMPLITUDE_THRESHOLD = 0.1F;
// NaN in float
inline constexpr auto PACKET_END = 0x00'00'C0'7FU;

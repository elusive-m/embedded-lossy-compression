import numpy as np
from numpy import sin, cos, pi
from numpy.typing import NDArray
from numpy.fft import rfft, irfft
import matplotlib.pyplot as plt
from matplotlib.widgets import Slider
from scipy import signal

# -------------
# Configuration
# -------------

# Minimum normalized amplitude of FFT window that isn't considered as noise
# Can be visualized on the fly
normalized_threshold = 0.10
# RNG determinism
SEED = 0xDEAD
# Sampling frequency
Fs = 1000
# Size of the window for the time-domain signal
WINDOW_SIZE = 64
# Duration of the signal
STOP_TIME = 0.5


# Input
def f(t: NDArray[np.float64]) -> NDArray[np.float64]:
    return (
        -1
        + 15 * signal.sawtooth(2 * pi * 25 * t)
        + 3 * cos(2 * pi * 50 * t)
        + 18 * sin(2 * pi * 30 * t + cos(2 * pi * 30 * t))
    )


# --------------
# Implementation
# --------------

np.random.seed(seed=SEED)

ts = 1 / Fs
t = np.arange(0, STOP_TIME, ts, dtype=np.float64)
t = t[: -(len(t) % WINDOW_SIZE)]

total_windows = len(t) // WINDOW_SIZE
f_windowed = [f(t) for t in np.split(t, total_windows)]
fft_windowed = rfft(f_windowed, axis=1)

abs_fft_windowed = np.abs(fft_windowed)
abs_fft_windowed = abs_fft_windowed / np.max(abs_fft_windowed, axis=1, keepdims=True)

# Compute the uncompressed size (if we sent the time-domain signal)
# Assuming IEEE-754/IEC-559
double_datatype_size = 8
uncompressed_size = double_datatype_size * len(t)


def reconstruct_lossy(threshold: float) -> NDArray[np.float64]:
    lossy_fft_windows = np.where(abs_fft_windowed >= threshold, fft_windowed, 0)
    return np.concatenate(irfft(lossy_fft_windows, axis=1).real)


def update_compression_percentage(threshold: float) -> None:
    non_zero_samples = np.count_nonzero(abs_fft_windowed >= threshold)

    frequency_index_size = 4
    end_of_frame_marker_size = frequency_index_size
    complex_datatype_size = double_datatype_size * 2

    compressed_size = (
        non_zero_samples * (frequency_index_size + complex_datatype_size)
        + total_windows * end_of_frame_marker_size
    )
    compression = 100 * (1 - compressed_size / uncompressed_size)

    fig.suptitle(f"Compression: {compression:.4}%")


# Showcase the differences in time-domain
fig, ax = plt.subplots()
update_compression_percentage(normalized_threshold)

plt.plot(t, f(t), "g", label="original")

reconstructed = reconstruct_lossy(normalized_threshold)
(lossy,) = ax.plot(t, reconstructed, "r", label="lossy")

ax.set_xlabel("Time [s]")
fig.subplots_adjust(bottom=0.20)

threshold_slider = Slider(
    ax=fig.add_axes([0.25, 0.05, 0.65, 0.03]),
    label="Threshold",
    valmin=0,
    valmax=1,
    valinit=0.1,
)


def update(threshold: float) -> None:
    update_compression_percentage(threshold)

    reconstructed = reconstruct_lossy(threshold)
    lossy.set_ydata(reconstructed)

    fig.canvas.draw_idle()


threshold_slider.on_changed(update)

ax.legend()
ax.set_xlim(t[0], t[-1])
plt.show()

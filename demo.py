import numpy as np
from numpy import sin, cos, pi
from numpy.typing import NDArray
from threading import Thread
from time import sleep, time
from serial import Serial, SerialException
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import struct

# Serial configuration
PORT: str = "<port>"
BAUD_RATE: int = 115_200
# Emit received frame information
VERBOSE: bool = True
# FFT size
WINDOW_SIZE: int = 64
# Sampling interval
Ts: float = 1 / 1000
# Time between frame updates (ms)
FRAME_INTERVAL: int = 30
# Streaming mode
STREAMING: bool = True
# How many previous samples to view
STREAMING_WINDOW: int = 8 * WINDOW_SIZE
# End of frame marker
EOF_MARKER: int = 0x00_00_C0_7F
# Signal timing parameters
START_TIME: float = 0.0
STOP_TIME: float = 6.0


# Signal function
def f(t: NDArray[np.float32]) -> NDArray[np.float32]:
    return 8 * cos(2 * pi * 20 * t) + 3 * sin(2 * pi * 80 * t) - 5


def main() -> None:
    try:
        serial = Serial(PORT, BAUD_RATE)
    except SerialException as e:
        print(f"Unable to open interface: {e}")
        return

    frames: list[NDArray[np.float64]] = []

    tx = Thread(target=transmitter, args=(serial,))
    rx = Thread(target=receiver, args=(serial, frames))

    rx.start()
    tx.start()

    fig = plt.figure()
    (ln1,) = plt.plot([], [], label="Reconstructed")
    (ln2,) = plt.plot([], [], label="Actual")

    def update(_):
        if not frames:
            return

        x = np.concatenate(frames)
        t = np.arange(START_TIME, len(x)) * Ts

        if STREAMING and len(t) >= STREAMING_WINDOW:
            t = t[-STREAMING_WINDOW:]
            x = x[-STREAMING_WINDOW:]

        ln1.set_data(t, x)
        ln2.set_data(t, f(t))

        fig.gca().relim()
        fig.gca().autoscale_view(tight=True)
        plt.legend(loc=2)

    _ = FuncAnimation(fig, update, interval=FRAME_INTERVAL)
    plt.legend()
    plt.show()


def transmitter(serial: Serial) -> None:
    # Generate time vector
    t = np.arange(START_TIME, STOP_TIME, Ts, dtype=np.float32)
    t = t[: -(len(t) % WINDOW_SIZE)]
    # Generate signal
    x = f(t)

    # Initial delay before TX
    sleep(0.5)
    start = time()

    for sample in x:
        raw_bytes = struct.pack("<f", sample)
        serial.write(raw_bytes)
        sleep(Ts)

    if VERBOSE:
        print("[!] End of transmission")


def receiver(serial: Serial, buffer: list[NDArray[np.float64]]) -> None:
    frame = np.zeros(1 + WINDOW_SIZE // 2, dtype=np.complex64)

    while True:
        data: bytes = serial.read(4)
        (idx,) = struct.unpack("<I", data)

        if idx != EOF_MARKER:
            data = serial.read(4)
            (real,) = struct.unpack("<f", data)

            data = serial.read(4)
            (imag,) = struct.unpack("<f", data)

            frame[idx] = real - 1j * imag
            if VERBOSE:
                print(f"[{idx}] {frame[idx]}")
        else:
            buffer.append(np.fft.irfft(frame).real)
            frame[:] = 0

            if VERBOSE:
                print(f"EOF")


if __name__ == "__main__":
    main()

// Adapted from: https://stackoverflow.com/a/23713793

#pragma once
#include <atomic>
#include <cstdint>

template <typename T> class double_buffer {
private:
  T buffers_[2]{};
  std::atomic<std::uint32_t> state_{0U};
  std::uint32_t read_state_{};

public:
  double_buffer() = default;
  ~double_buffer() = default;

  // Never returns nullptr
  T &start_writing() noexcept {
    // Increment active users; once we do this, no one
    // can swap the active cell on us until we're done
    auto const state = state_.fetch_add(0x2U, std::memory_order_relaxed);
    return buffers_[state & 1U];
  }

  void stop_writing() noexcept {
    // We want to swap the active cell, but only if we were the last
    // ones concurrently accessing the data (otherwise the consumer
    // will do it for us when *it's* done accessing the data)
    auto state = state_.load(std::memory_order_relaxed);
    auto const flag = (8U << (state & 1U)) ^ (state & (8U << (state & 1U)));
    state =
        state_.fetch_add(flag - 0x2U, std::memory_order_release) + flag - 0x2U;

    if ((state & 0x6U) == 0) {
      // The consumer wasn't in the middle of a read, we should
      // swap (unless the consumer has since started a read or
      // already swapped or read a value and is about to swap).
      // If we swap, we also want to clear the full flag on what
      // will become the active cell, otherwise the consumer could
      // eventually read two values out of order (it reads a new
      // value, then swaps and reads the old value while the
      // producer is idle).
      state_.compare_exchange_strong(state,
                                     (state ^ 0x1U) & ~(0x10U >> (state & 1U)),
                                     std::memory_order_release);
    }
  }

  // Returns nullptr if there appears to be no more data to read yet
  T *start_reading() noexcept {
    read_state_ = state_.load(std::memory_order_relaxed);
    if ((read_state_ & (0x10U >> (read_state_ & 1U))) == 0U) {
      // Nothing to read here!
      return nullptr;
    }

    // At this point, there is guaranteed to be something to
    // read, because the full flag is never turned off by the
    // producer thread once it's on; the only thing that could
    // happen is that the active cell changes, but that can
    // only happen after the producer wrote a value into it,
    // in which case there's still a value to read, just in a
    // different cell.
    read_state_ = state_.fetch_add(0x2U, std::memory_order_acquire) + 0x2U;

    // Now that we've incremented the user count, nobody can swap until
    // we decrement it
    return &buffers_[(read_state_ & 1U) ^ 1U];
  }

  void end_reading() noexcept {
    if ((read_state_ & (0x10U >> (read_state_ & 1U))) == 0) {
      // There was nothing to read; shame to repeat this
      // check, but if these functions are inlined it might
      // not matter. Otherwise the API could be changed.
      // Or just don't call this method if start_reading()
      // returns nullptr -- then you could also get rid
      // of read_state_.
      return;
    }

    // Alright, at this point the active cell cannot change on
    // us, but the active cell's flag could change and the user
    // count could change. We want to release our user count
    // and remove the flag on the value we read.

    auto state = state_.load(std::memory_order_relaxed);
    auto const sub = (0x10U >> (state & 1U)) | 0x2U;

    state = state_.fetch_sub(sub, std::memory_order_relaxed) - sub;
    if ((state & 0x6U) == 0U && (state & (0x8U << (state & 1U))) == 1U) {
      // Oi, we were the last ones accessing the data when we released our cell.
      // That means we should swap, but only if the producer isn't in the middle
      // of producing something, and hasn't already swapped, and hasn't already
      // set the flag we just reset (which would mean they swapped an even
      // number of times).  Note that we don't bother swapping if there's
      // nothing to read in the other cell.
      state_.compare_exchange_strong(state, state ^ 0x1U,
                                     std::memory_order_relaxed);
    }
  }

  T &raw_get(std::size_t n) noexcept { return buffers_[n]; }
};

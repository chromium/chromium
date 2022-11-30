// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_MONITORED_VALUE_H_
#define IPCZ_SRC_IPCZ_MONITORED_VALUE_H_

#include <atomic>
#include <limits>
#include <type_traits>

namespace ipcz {

// MonitoredAtomic is a trivial wrapper around around an atomic unsigned
// integral value, with the high bit reserved for primitive communication
// between one producer and any number of concurrent consumers of the value.
//
// Consumers can atomically query the value while simultaneously signaling that
// they want to be notified about the next time the value changes. Producers can
// atomically update the value while simulataneously querying (and resetting)
// the consumer's interest in being notified about the change.
template <typename T>
class MonitoredAtomic {
  static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>,
                "MonitoredAtomic requires an unsigned integral type");

 public:
  struct State {
    T value;
    bool monitored;
  };

  static constexpr T kMaxValue = std::numeric_limits<T>::max() >> 1;
  static constexpr T kMonitorBit = kMaxValue + 1;

  MonitoredAtomic() noexcept = default;
  explicit MonitoredAtomic(T value) noexcept : value_(value) {}

  // Returns a best-effort snapshot of the most recent underlying value. If
  // `monitor` is true in `options`, then the stored value is also atomically
  // flagged for monitoring.
  struct QueryOptions {
    bool monitor;
  };
  State Query(const QueryOptions& options) {
    T value = value_.load(std::memory_order_relaxed);
    while (options.monitor && !IsMonitored(value) &&
           !value_.compare_exchange_weak(value, Monitored(value),
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {
    }
    return {.value = Unmonitored(value), .monitored = IsMonitored(value)};
  }

  // Stores a new underlying value, resetting the monitor bit if it was set.
  // Returns a boolean indicating whether the monitor bit was set.
  [[nodiscard]] bool UpdateValueAndResetMonitor(T value) {
    T old_value = value_.load(std::memory_order_relaxed);
    while (value != old_value &&
           !value_.compare_exchange_weak(old_value, value,
                                         std::memory_order_release,
                                         std::memory_order_relaxed)) {
    }
    return IsMonitored(old_value);
  }

 private:
  static bool IsMonitored(T value) { return (value & kMonitorBit) != 0; }
  static T Monitored(T value) { return value | kMonitorBit; }
  static T Unmonitored(T value) { return value & kMaxValue; }

  std::atomic<T> value_{0};
};

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_MONITORED_VALUE_H_

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once
#include "core/common/spin_pause.h"
#include <atomic>

namespace onnxruntime {
/*
OrtSpinLock implemented mutex semantic "lock-freely",
calling thread will not be put to sleep on blocked,
which reduces cpu usage on context switching.
*/
struct OrtSpinLock {
  using LockState = enum { Locked = 0,
                           Unlocked };

  void lock() noexcept {
    LockState state = Unlocked;
    while (!state_.compare_exchange_weak(state, Locked, std::memory_order_acq_rel, std::memory_order_relaxed)) {
      state = Unlocked;
      concurrency::SpinPause();  // pause and retry
    }
  }
  bool try_lock() noexcept {
    LockState state = Unlocked;
    return state_.compare_exchange_weak(state, Locked, std::memory_order_acq_rel, std::memory_order_relaxed);
  }
  void unlock() noexcept {
    state_.store(Unlocked, std::memory_order_release);
  }

 private:
  std::atomic<LockState> state_{Unlocked};
};
}  // namespace onnxruntime
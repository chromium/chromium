// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2016 Dmitry Vyukov <dvyukov@google.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <assert.h>

#include "core/common/spin_pause.h"

#include <mutex>
#include <condition_variable>
#include <atomic>

namespace onnxruntime {
class Barrier {
 public:
  explicit Barrier(unsigned int count, bool spin = false)
      : state_(count << 1), notified_(false), spin_(spin) {
    assert(((count << 1) >> 1) == count);
  }
#ifdef NDEBUG
  ~Barrier() = default;
#else
  ~Barrier() {
    assert((state_ >> 1) == 0);
  }
#endif

  void Notify(unsigned int c = 1) {
    unsigned int delta = c << 1;
    unsigned int v = state_.fetch_sub(delta, std::memory_order_acq_rel) - delta;
    if (v != 1) {
      // Clear the lowest bit (waiter flag) and check that the original state
      // value was not zero. If it was zero, it means that notify was called
      // more times than the original count.
      assert(((v + delta) & ~1) != 0);
      return;  // either count has not dropped to 0, or waiter is not waiting
    }
    std::unique_lock<std::mutex> l(mu_);
    assert(!notified_);
    notified_ = true;
    cv_.notify_all();
  }

  void Wait() {
    if (spin_) {
      while ((state_ >> 1) != 0) {
        onnxruntime::concurrency::SpinPause();
      }
    } else {
      unsigned int v = state_.fetch_or(1, std::memory_order_acq_rel);
      if ((v >> 1) == 0)
        return;
      std::unique_lock<std::mutex> l(mu_);
      while (!notified_) {
        cv_.wait(l);
      }
    }
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  std::atomic<unsigned int> state_;  // low bit is waiter flag
  bool notified_;
  const bool spin_;
};

// Notification is an object that allows a user to to wait for another
// thread to signal a notification that an event has occurred.
//
// Multiple threads can wait on the same Notification object,
// but only one caller must call Notify() on the object.
struct Notification : Barrier {
  Notification() : Barrier(1) {};
};
}  // namespace onnxruntime

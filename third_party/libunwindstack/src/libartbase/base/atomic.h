/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBARTBASE_BASE_ATOMIC_H_
#define ART_LIBARTBASE_BASE_ATOMIC_H_

#include <stdint.h>
#include <atomic>
#include <limits>
#include <vector>

#include <android-base/logging.h>

#include "macros.h"

namespace art {

enum class CASMode {
  kStrong,
  kWeak,
};

template<typename T>
class PACKED(sizeof(T)) Atomic : public std::atomic<T> {
 public:
  Atomic<T>() : std::atomic<T>(T()) { }

  explicit Atomic<T>(T value) : std::atomic<T>(value) { }

  // Load data from an atomic variable with Java data memory order semantics.
  //
  // Promises memory access semantics of ordinary Java data.
  // Does not order other memory accesses.
  // Long and double accesses may be performed 32 bits at a time.
  // There are no "cache coherence" guarantees; e.g. loads from the same location may be reordered.
  // In contrast to normal C++ accesses, racing accesses are allowed.
  T LoadJavaData() const {
    return this->load(std::memory_order_relaxed);
  }

  // Store data in an atomic variable with Java data memory ordering semantics.
  //
  // Promises memory access semantics of ordinary Java data.
  // Does not order other memory accesses.
  // Long and double accesses may be performed 32 bits at a time.
  // There are no "cache coherence" guarantees; e.g. loads from the same location may be reordered.
  // In contrast to normal C++ accesses, racing accesses are allowed.
  void StoreJavaData(T desired_value) {
    this->store(desired_value, std::memory_order_relaxed);
  }

  // Atomically replace the value with desired_value if it matches the expected_value.
  // Participates in total ordering of atomic operations.
  bool CompareAndSetStrongSequentiallyConsistent(T expected_value, T desired_value) {
    return this->compare_exchange_strong(expected_value, desired_value, std::memory_order_seq_cst);
  }

  // The same, except it may fail spuriously.
  bool CompareAndSetWeakSequentiallyConsistent(T expected_value, T desired_value) {
    return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_seq_cst);
  }

  // Atomically replace the value with desired_value if it matches the expected_value. Doesn't
  // imply ordering or synchronization constraints.
  bool CompareAndSetStrongRelaxed(T expected_value, T desired_value) {
    return this->compare_exchange_strong(expected_value, desired_value, std::memory_order_relaxed);
  }

  // Atomically replace the value with desired_value if it matches the expected_value. Prior writes
  // to other memory locations become visible to the threads that do a consume or an acquire on the
  // same location.
  bool CompareAndSetStrongRelease(T expected_value, T desired_value) {
    return this->compare_exchange_strong(expected_value, desired_value, std::memory_order_release);
  }

  // The same, except it may fail spuriously.
  bool CompareAndSetWeakRelaxed(T expected_value, T desired_value) {
    return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_relaxed);
  }

  // Atomically replace the value with desired_value if it matches the expected_value. Prior writes
  // made to other memory locations by the thread that did the release become visible in this
  // thread.
  bool CompareAndSetWeakAcquire(T expected_value, T desired_value) {
    return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_acquire);
  }

  // Atomically replace the value with desired_value if it matches the expected_value. Prior writes
  // to other memory locations become visible to the threads that do a consume or an acquire on the
  // same location.
  bool CompareAndSetWeakRelease(T expected_value, T desired_value) {
    return this->compare_exchange_weak(expected_value, desired_value, std::memory_order_release);
  }

  bool CompareAndSet(T expected_value,
                     T desired_value,
                     CASMode mode,
                     std::memory_order memory_order) {
    return mode == CASMode::kStrong
        ? this->compare_exchange_strong(expected_value, desired_value, memory_order)
        : this->compare_exchange_weak(expected_value, desired_value, memory_order);
  }

  // Returns the address of the current atomic variable. This is only used by futex() which is
  // declared to take a volatile address (see base/mutex-inl.h).
  volatile T* Address() {
    return reinterpret_cast<T*>(this);
  }

  static T MaxValue() {
    return std::numeric_limits<T>::max();
  }
};

// Increment a debug- or statistics-only counter when there is a single writer, especially if
// concurrent reads are uncommon. Usually appreciably faster in this case.
// NOT suitable as an approximate counter with multiple writers.
template <typename T>
void IncrementStatsCounter(std::atomic<T>* a) {
  a->store(a->load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
}

using AtomicInteger = Atomic<int32_t>;

static_assert(sizeof(AtomicInteger) == sizeof(int32_t), "Weird AtomicInteger size");
static_assert(alignof(AtomicInteger) == alignof(int32_t),
              "AtomicInteger alignment differs from that of underlyingtype");
static_assert(sizeof(Atomic<int64_t>) == sizeof(int64_t), "Weird Atomic<int64> size");

// Assert the alignment of 64-bit integers is 64-bit. This isn't true on certain 32-bit
// architectures (e.g. x86-32) but we know that 64-bit integers here are arranged to be 8-byte
// aligned.
#if defined(__LP64__)
  static_assert(alignof(Atomic<int64_t>) == alignof(int64_t),
                "Atomic<int64> alignment differs from that of underlying type");
#endif

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_ATOMIC_H_

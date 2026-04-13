// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dcomp_surface_registry.h"

#include <windows.h>

#include <atomic>
#include <thread>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gl {

namespace {

class DCOMPSurfaceRegistryTest : public testing::Test {
 public:
  void SetUp() override { registry_ = DCOMPSurfaceRegistry::GetInstance(); }

 protected:
  raw_ptr<DCOMPSurfaceRegistry> registry_;
};

}  // namespace

// Stress test for concurrent access to DCOMPSurfaceRegistry using the
// barrier pattern to ensure TSAN consistently catches data races.
//
// Without proper synchronization (e.g., base::Lock), this test would likely
// fail in the following ways:
// 1. Memory Corruption (UAF/HeapBOf): base::flat_map uses a contiguous
//    std::vector. If one thread triggers a reallocation during an insertion
//    while another thread is searching or erasing, the latter will hold an
//    invalidated iterator or pointer.
// 2. Container Inconsistency: Concurrent insertions and erasures can leave
//    the map in an unsorted or corrupted state, leading to failed lookups
//    for valid tokens.
// 3. Sanitizer Triggers: ASan would detect container-overflow or
//    heap-use-after-free, and TSan would flag a data race.
TEST_F(DCOMPSurfaceRegistryTest, ConcurrentRegisterAndTake) {
  const int kOpsPerThread = 100;

  std::vector<base::UnguessableToken> tokens;
  base::Lock tokens_lock;

  std::atomic<bool> start_flag{false};
  std::atomic<int> threads_ready{0};

  auto register_worker = [&]() {
    threads_ready++;
    while (!start_flag.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    for (int i = 0; i < kOpsPerThread; ++i) {
      base::win::ScopedHandle handle(
          ::CreateEvent(nullptr, FALSE, FALSE, nullptr));
      base::UnguessableToken token =
          registry_->RegisterDCOMPSurfaceHandle(std::move(handle));
      {
        base::AutoLock lock(tokens_lock);
        tokens.push_back(token);
      }
    }
  };

  auto take_worker = [&]() {
    threads_ready++;
    while (!start_flag.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    int taken = 0;
    while (taken < kOpsPerThread) {
      base::UnguessableToken token;
      {
        base::AutoLock lock(tokens_lock);
        if (!tokens.empty()) {
          token = tokens.back();
          tokens.pop_back();
        }
      }
      if (!token.is_empty()) {
        base::win::ScopedHandle handle =
            registry_->TakeDCOMPSurfaceHandle(token);
        taken++;
      } else {
        std::this_thread::yield();
      }
    }
  };

  // With the barrier pattern, two threads are sufficient to trigger
  // the race condition for TSAN.
  std::thread t1(register_worker);
  std::thread t2(take_worker);

  // Wait until both threads are ready at the starting line.
  while (threads_ready.load(std::memory_order_relaxed) < 2) {
    std::this_thread::yield();
  }

  // Signal the staring flag to allow both threads to race from the initialized
  // state.
  start_flag.store(true, std::memory_order_release);

  t1.join();
  t2.join();
}

}  // namespace gl

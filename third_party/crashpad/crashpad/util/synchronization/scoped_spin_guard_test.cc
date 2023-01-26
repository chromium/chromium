// Copyright 2022 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/synchronization/scoped_spin_guard.h"

#include <optional>
#include <thread>

#include "gtest/gtest.h"
#include "util/misc/clock.h"

namespace crashpad {
namespace test {
namespace {

TEST(ScopedSpinGuard, TryCreateScopedSpinGuardShouldLockStateWhileInScope) {
  SpinGuardState s;
  EXPECT_FALSE(s.locked);
  {
    std::optional<ScopedSpinGuard> guard =
        ScopedSpinGuard::TryCreateScopedSpinGuard(/*timeout_nanos=*/0, s);
    EXPECT_NE(std::nullopt, guard);
    EXPECT_TRUE(s.locked);
  }
  EXPECT_FALSE(s.locked);
}

TEST(
    ScopedSpinGuard,
    TryCreateScopedSpinGuardWithZeroTimeoutShouldFailImmediatelyIfStateLocked) {
  SpinGuardState s;
  s.locked = true;
  std::optional<ScopedSpinGuard> guard =
      ScopedSpinGuard::TryCreateScopedSpinGuard(/*timeout_nanos=*/0, s);
  EXPECT_EQ(std::nullopt, guard);
  EXPECT_TRUE(s.locked);
}

TEST(
    ScopedSpinGuard,
    TryCreateScopedSpinGuardWithNonZeroTimeoutShouldSucceedIfStateUnlockedDuringTimeout) {
  SpinGuardState s;
  s.locked = true;
  std::thread unlock_thread([&s] {
    constexpr uint64_t kUnlockThreadSleepTimeNanos = 10000;  // 10 us
    EXPECT_TRUE(s.locked);
    SleepNanoseconds(kUnlockThreadSleepTimeNanos);
    s.locked = false;
  });
  constexpr uint64_t kLockThreadTimeoutNanos = 180000000000ULL;  // 3 minutes
  std::optional<ScopedSpinGuard> guard =
      ScopedSpinGuard::TryCreateScopedSpinGuard(kLockThreadTimeoutNanos, s);
  EXPECT_NE(std::nullopt, guard);
  EXPECT_TRUE(s.locked);
  unlock_thread.join();
}

TEST(ScopedSpinGuard, SwapShouldMaintainSpinLock) {
  SpinGuardState s;
  std::optional<ScopedSpinGuard> outer_guard;
  EXPECT_EQ(std::nullopt, outer_guard);
  {
    std::optional<ScopedSpinGuard> inner_guard =
        ScopedSpinGuard::TryCreateScopedSpinGuard(/*timeout_nanos=*/0, s);
    EXPECT_NE(std::nullopt, inner_guard);
    EXPECT_TRUE(s.locked);
    // If the move-assignment operator for `ScopedSpinGuard` is implemented
    // incorrectly (e.g., the `= default` implementation), `inner_guard`
    // will incorrectly think it still "owns" the spinlock after the swap,
    // and when it falls out of scope, it will release the lock prematurely
    // when it destructs.
    //
    // Confirm that the spinlock stays locked even after the swap.
    std::swap(outer_guard, inner_guard);
    EXPECT_TRUE(s.locked);
    EXPECT_EQ(std::nullopt, inner_guard);
  }
  EXPECT_NE(std::nullopt, outer_guard);
  EXPECT_TRUE(s.locked);
}

TEST(ScopedSpinGuard, MoveAssignmentShouldMaintainSpinLock) {
  SpinGuardState s;
  std::optional<ScopedSpinGuard> outer_guard;
  EXPECT_EQ(std::nullopt, outer_guard);
  {
    outer_guard =
        ScopedSpinGuard::TryCreateScopedSpinGuard(/*timeout_nanos=*/0, s);
    EXPECT_NE(std::nullopt, outer_guard);
    EXPECT_TRUE(s.locked);
  }
  EXPECT_NE(std::nullopt, outer_guard);
  EXPECT_TRUE(s.locked);
}

}  // namespace
}  // namespace test
}  // namespace crashpad

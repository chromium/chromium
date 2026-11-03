// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/scoped_input_method_manager_for_testing.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"

namespace ash::input_method {
namespace {

// Takes a constructor argument, to check that it gets forwarded, and counts
// how many times it is destroyed.
class CountingInputMethodManager : public MockInputMethodManager {
 public:
  explicit CountingInputMethodManager(int* destroyed_count)
      : destroyed_count_(destroyed_count) {}
  ~CountingInputMethodManager() override { ++(*destroyed_count_); }

 private:
  raw_ptr<int> destroyed_count_;
};

TEST(ScopedInputMethodManagerForTestingTest, InstallsAndShutsDown) {
  ASSERT_FALSE(InputMethodManager::Get());
  int destroyed_count = 0;
  {
    ScopedInputMethodManagerForTesting<CountingInputMethodManager> manager(
        &destroyed_count);
    EXPECT_EQ(InputMethodManager::Get(), manager.get());
    EXPECT_EQ(destroyed_count, 0);
  }
  EXPECT_FALSE(InputMethodManager::Get());
  EXPECT_EQ(destroyed_count, 1);
}

TEST(ScopedInputMethodManagerForTestingTest, CrashesIfAlreadyInstalled) {
  ASSERT_FALSE(InputMethodManager::Get());
  ScopedInputMethodManagerForTesting<MockInputMethodManager> manager;
  EXPECT_CHECK_DEATH(
      { ScopedInputMethodManagerForTesting<MockInputMethodManager> second; });
}

}  // namespace
}  // namespace ash::input_method

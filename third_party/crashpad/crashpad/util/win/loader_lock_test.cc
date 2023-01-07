// Copyright 2019 The Crashpad Authors
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

#include "util/win/loader_lock.h"

#include "gtest/gtest.h"
#include "util/win/get_function.h"

extern "C" bool LoaderLockDetected();

namespace crashpad {
namespace test {
namespace {

TEST(LoaderLock, Detected) {
  EXPECT_FALSE(IsThreadInLoaderLock());
  auto* loader_lock_detected = GET_FUNCTION_REQUIRED(
      L"crashpad_util_test_loader_lock_test.dll", LoaderLockDetected);
  EXPECT_TRUE(loader_lock_detected());
  EXPECT_FALSE(IsThreadInLoaderLock());
}

}  // namespace
}  // namespace test
}  // namespace crashpad

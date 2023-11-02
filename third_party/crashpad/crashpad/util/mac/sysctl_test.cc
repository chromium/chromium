// Copyright 2020 The Crashpad Authors
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

#include "util/mac/sysctl.h"

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(Sysctl, ReadStringSysctlByName) {
  // kern.ostype is always provided by the kernel, and it’s a constant across
  // all versions, so it makes for a good test.
  EXPECT_EQ(ReadStringSysctlByName("kern.ostype", true), "Darwin");

  // Names expected to not exist.
  EXPECT_TRUE(ReadStringSysctlByName("kern.scheisskopf", true).empty());
  EXPECT_TRUE(ReadStringSysctlByName("kern.sanders", false).empty());
}

}  // namespace
}  // namespace test
}  // namespace crashpad

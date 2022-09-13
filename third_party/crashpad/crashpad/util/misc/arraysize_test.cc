// Copyright 2016 The Crashpad Authors
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

#include "util/misc/arraysize.h"

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(ArraySize, ArraySize) {
  [[maybe_unused]] char c1[1];
  static_assert(ArraySize(c1) == 1, "c1");

  [[maybe_unused]] char c2[2];
  static_assert(ArraySize(c2) == 2, "c2");

  [[maybe_unused]] char c4[4];
  static_assert(ArraySize(c4) == 4, "c4");

  [[maybe_unused]] int i1[1];
  static_assert(ArraySize(i1) == 1, "i1");

  [[maybe_unused]] int i2[2];
  static_assert(ArraySize(i2) == 2, "i2");

  [[maybe_unused]] int i4[4];
  static_assert(ArraySize(i4) == 4, "i4");

  [[maybe_unused]] long l8[8];
  static_assert(ArraySize(l8) == 8, "l8");

  [[maybe_unused]] int l9[9];
  static_assert(ArraySize(l9) == 9, "l9");

  struct S {
    char c;
    int i;
    long l;
    bool b;
  };

  [[maybe_unused]] S s1[1];
  static_assert(ArraySize(s1) == 1, "s1");

  [[maybe_unused]] S s10[10];
  static_assert(ArraySize(s10) == 10, "s10");
}

}  // namespace
}  // namespace test
}  // namespace crashpad

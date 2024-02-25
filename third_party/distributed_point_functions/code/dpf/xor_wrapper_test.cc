// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dpf/xor_wrapper.h"

#include <stdint.h>

#include "absl/numeric/int128.h"
#include "gtest/gtest.h"

namespace distributed_point_functions {

namespace {

template <typename T>
class XorWrapperTest : public testing::Test {};
using XorWrapperTypes =
    testing::Types<uint8_t, uint16_t, uint32_t, uint64_t, absl::uint128>;

TYPED_TEST_SUITE(XorWrapperTest, XorWrapperTypes);

TYPED_TEST(XorWrapperTest, TestConstructor) {
  TypeParam value{42};

  XorWrapper<TypeParam> wrapper(value);

  EXPECT_EQ(wrapper.value(), value);
}

TYPED_TEST(XorWrapperTest, TestAddition) {
  TypeParam a{42}, b{23};
  XorWrapper<TypeParam> wrapped_a(a), wrapped_b(b);

  EXPECT_EQ((wrapped_a + wrapped_b).value(), a ^ b);
}

TYPED_TEST(XorWrapperTest, TestSubtraction) {
  TypeParam a{42}, b{23};
  XorWrapper<TypeParam> wrapped_a(a), wrapped_b(b);

  EXPECT_EQ((wrapped_a - wrapped_b).value(), a ^ b);
}

TYPED_TEST(XorWrapperTest, TestNegation) {
  TypeParam value{42};
  XorWrapper<TypeParam> wrapper(value);

  EXPECT_EQ((-wrapper).value(), value);
}

TYPED_TEST(XorWrapperTest, TestEquality) {
  TypeParam a{42}, b{23};
  XorWrapper<TypeParam> wrapped_a(a), wrapped_b(b);

  EXPECT_EQ(wrapped_a, XorWrapper<TypeParam>(a));
  EXPECT_NE(wrapped_a, XorWrapper<TypeParam>(b));
}

}  // namespace

}  // namespace distributed_point_functions

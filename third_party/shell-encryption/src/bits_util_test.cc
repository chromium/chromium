// Copyright 2020 Google LLC
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

#include "bits_util.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/numeric/int128.h"

using ::testing::Eq;

namespace {

TEST(BitsUtilTest, CountOnes64Works) {
  EXPECT_THAT(rlwe::internal::CountOnes64(0xFF000000000000FF), Eq(16));
  EXPECT_THAT(rlwe::internal::CountOnes64(0xFF000000000000FE), Eq(15));
  EXPECT_THAT(rlwe::internal::CountOnes64(0xFF0000000000FF00), Eq(16));
  EXPECT_THAT(rlwe::internal::CountOnes64(0x1111111111111111), Eq(16));
  EXPECT_THAT(rlwe::internal::CountOnes64(0x0321212121212121), Eq(16));
}

TEST(BitsUtilTest, CountOnesInByte) {
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0x00), Eq(0));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0x01), Eq(1));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0x11), Eq(2));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0x22), Eq(2));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0x44), Eq(2));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0xFF), Eq(8));
  EXPECT_THAT(rlwe::internal::CountOnesInByte(0xEE), Eq(6));
}

TEST(BitsUtilTest, CountLeadingZeros64Works) {
  rlwe::Uint64 value = 0x8000000000000000;
  for (int i = 0; i < 64; i++) {
    EXPECT_THAT(rlwe::internal::CountLeadingZeros64(value), Eq(i));
    value >>= 1;
  }
}

TEST(BitsUtilTest, BitLengthWorks) {
  absl::uint128 value = absl::MakeUint128(0x8000000000000000, 0);
  for (int i = 0; i <= 128; i++) {
    EXPECT_THAT(rlwe::internal::BitLength(value), Eq(128 - i));
    value >>= 1;
  }
}

}  // namespace

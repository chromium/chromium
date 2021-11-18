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

#include "dpf/internal/array_conversions.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace distributed_point_functions {
namespace dpf_internal {
namespace {

using ::testing::ElementsAre;

TEST(ArrayConversionTest, TestUint128ToArray) {
  absl::uint128 block =
      absl::MakeUint128(0x0f0e0d0c0b0a0908, 0x0706050403020100);
  EXPECT_THAT(Uint128ToArray<uint8_t>(block),
              ElementsAre(0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                          0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F));
  EXPECT_THAT(Uint128ToArray<uint16_t>(block),
              ElementsAre(0x0100, 0x0302, 0x0504, 0x0706, 0x0908, 0x0b0a,
                          0x0d0c, 0x0f0e));
  EXPECT_THAT(Uint128ToArray<uint32_t>(block),
              ElementsAre(0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c));
  EXPECT_THAT(Uint128ToArray<uint64_t>(block),
              ElementsAre(0x0706050403020100, 0x0f0e0d0c0b0a0908));
  EXPECT_THAT(Uint128ToArray<absl::uint128>(block), ElementsAre(block));
}

TEST(ArrayConversionTest, TestArrayToUint128) {
  absl::uint128 expected =
      absl::MakeUint128(0x0f0e0d0c0b0a0908, 0x0706050403020100);
  EXPECT_EQ(
      ArrayToUint128<uint8_t>({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                               0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}),
      expected);
  EXPECT_EQ(ArrayToUint128<uint16_t>({0x0100, 0x0302, 0x0504, 0x0706, 0x0908,
                                      0x0b0a, 0x0d0c, 0x0f0e}),
            expected);
  EXPECT_EQ(ArrayToUint128<uint32_t>(
                {0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c}),
            expected);
  EXPECT_EQ(ArrayToUint128<uint64_t>({0x0706050403020100, 0x0f0e0d0c0b0a0908}),
            expected);
  EXPECT_EQ(ArrayToUint128<absl::uint128>({expected}), expected);
}

}  // namespace

}  // namespace dpf_internal
}  // namespace distributed_point_functions

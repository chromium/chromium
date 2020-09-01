// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "third_party/private_membership/src/internal/utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "third_party/shell-encryption/src/testing/status_matchers.h"
#include "third_party/shell-encryption/src/testing/status_testing.h"

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

namespace private_membership {
namespace rlwe {
namespace {

TEST(UtilsTest, TruncateBitLengthZero) {
  ASSERT_OK_AND_ASSIGN(auto x, Truncate("\x01\x01\x01\x01", 0));
  EXPECT_EQ(x, "");
}

TEST(UtilsTest, TruncateBitLengthExceed) {
  EXPECT_THAT(Truncate("\x01\x01\x01\x01", 33),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Truncation bit length out of bounds.")));
}

TEST(UtilsTest, TruncateBitLengthExact) {
  ASSERT_OK_AND_ASSIGN(auto x, Truncate("\x01\x01\x01\xFF", 32));
  EXPECT_EQ(x, "\x01\x01\x01\xFF");
}

TEST(UtilsTest, TruncateBitLengthDivisibleBy8) {
  ASSERT_OK_AND_ASSIGN(auto x, Truncate("\x01\x01\xFF\xFF", 24));
  EXPECT_EQ(x, "\x01\x01\xFF");
}

TEST(UtilsTest, TruncateBitLengthNotDivisibleBy8) {
  ASSERT_OK_AND_ASSIGN(auto x, Truncate("\x01\x01\xFF\xFE", 30));
  EXPECT_EQ(x, "\x01\x01\xFF\xFC");
}

TEST(UtilsTest, TruncateAsUint32BitLengthExceedInput) {
  EXPECT_THAT(TruncateAsUint32("\x01\x01\x01", 25),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Truncation bit length out of bounds.")));
}

TEST(UtilsTest, TruncateAsUint32BitLengthExceed32) {
  EXPECT_THAT(TruncateAsUint32("\x01\x01\x01\x01\x01", 33),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Input bit length larger than 32 bits.")));
}

TEST(UtilsTest, TruncateAsUint32BitLengthExactly32) {
  ASSERT_OK_AND_ASSIGN(auto x1, TruncateAsUint32("\x01\x01\x01\xFF", 32));
  EXPECT_EQ(x1, 16843263);

  ASSERT_OK_AND_ASSIGN(auto x2, TruncateAsUint32("\xFF\xFFx\xFF\xFF", 32));
  EXPECT_EQ(x2, 4294932735LL);
}

TEST(UtilsTest, TruncateAsUint32BitLengthLessThan32) {
  // Truncating first 22 bits gives 0000000100000001111111 in binary, which
  // is equivalent to 16511 in decimal.
  ASSERT_OK_AND_ASSIGN(auto x, TruncateAsUint32("\x01\x01\xFF\xFF", 22));
  EXPECT_EQ(x, 16511);
}

TEST(UtilsTest, IsValidFalse) { EXPECT_FALSE(IsValid("\x01\x01\x01\x01", 24)); }

TEST(UtilsTest, IsValidTrue) { EXPECT_TRUE(IsValid("\x01\x01\x01\x10", 28)); }

}  // namespace
}  // namespace rlwe
}  // namespace private_membership

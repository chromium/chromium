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

#include "third_party/private_membership/src/internal/oprf_utils.h"

#include "third_party/private-join-and-compute/src/crypto/ec_commutative_cipher.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "third_party/shell-encryption/src/testing/status_matchers.h"
#include "third_party/shell-encryption/src/testing/status_testing.h"

namespace private_membership {
namespace {

using ::rlwe::testing::StatusIs;

constexpr int kCurveId = NID_X9_62_prime256v1;

TEST(OprfUtilsTest, ReEncryptIdSuccess) {
  ASSERT_OK_AND_ASSIGN(
      auto ec_cipher1,
      private_join_and_compute::ECCommutativeCipher::CreateWithNewKey(
          kCurveId, private_join_and_compute::ECCommutativeCipher::HashType::SHA256));
  ASSERT_OK_AND_ASSIGN(
      auto ec_cipher2,
      private_join_and_compute::ECCommutativeCipher::CreateWithNewKey(
          kCurveId, private_join_and_compute::ECCommutativeCipher::HashType::SHA256));
  ASSERT_OK_AND_ASSIGN(auto encrypted_id, ec_cipher1->Encrypt("plaintext id"));

  ASSERT_OK_AND_ASSIGN(auto doubly_encrypted_id,
                       ReEncryptId(encrypted_id, ec_cipher2.get()));
  EXPECT_EQ(doubly_encrypted_id.queried_encrypted_id(), encrypted_id);
  ASSERT_OK_AND_ASSIGN(auto result, ec_cipher2->ReEncrypt(encrypted_id));
  EXPECT_EQ(doubly_encrypted_id.doubly_encrypted_id(), result);
}

TEST(OprfUtilsTest, ReEncryptIdFail) {
  ASSERT_OK_AND_ASSIGN(
      auto ec_cipher,
      private_join_and_compute::ECCommutativeCipher::CreateWithNewKey(
          kCurveId, private_join_and_compute::ECCommutativeCipher::HashType::SHA256));
  // Empty substring necessary due to forked StatusIs.
  EXPECT_THAT(
      ReEncryptId("not encrypted id", ec_cipher.get()),
      StatusIs(absl::StatusCode::kInvalidArgument, testing::HasSubstr("")));
}

}  // namespace
}  // namespace private_membership

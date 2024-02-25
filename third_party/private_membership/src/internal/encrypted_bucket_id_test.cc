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

#include "third_party/private_membership/src/internal/encrypted_bucket_id.h"

#include <cstdint>
#include <string>

#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/internal/rlwe_id_utils.h"
#include "third_party/private_membership/src/internal/testing/constants.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "third_party/shell-encryption/src/testing/protobuf_matchers.h"
#include "third_party/shell-encryption/src/testing/status_matchers.h"
#include "third_party/shell-encryption/src/testing/status_testing.h"

namespace private_membership {
namespace rlwe {
namespace {

using ::rlwe::testing::EqualsProto;
using ::rlwe::testing::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Ne;

TEST(EncryptedBucketIdTest, CreateError) {
  EXPECT_THAT(EncryptedBucketId::Create("abcd", 33),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid bit_length.")));
}

TEST(EncryptedBucketIdTest, CreateSuccess) {
  EXPECT_OK(EncryptedBucketId::Create("abcd", 32));
}

TEST(EncryptedBucketIdTest, CreateWithHashingSuccess) {
  EncryptedBucketsParameters params;
  params.set_encrypted_bucket_id_length(14);

  RlwePlaintextId plaintext_id;
  plaintext_id.set_non_sensitive_id("nsid");
  plaintext_id.set_sensitive_id("sid");

  ::private_join_and_compute::Context ctx;
  ASSERT_OK_AND_ASSIGN(
      auto ec_cipher,
      ::private_join_and_compute::ECCommutativeCipher::CreateWithNewKey(
          kTestCurveId, ::private_join_and_compute::ECCommutativeCipher::HashType::SHA256));

  std::string full_id = HashRlwePlaintextId(plaintext_id);
  ASSERT_OK_AND_ASSIGN(std::string encrypted_id, ec_cipher->Encrypt(full_id));

  ASSERT_OK_AND_ASSIGN(
      auto bucket1,
      EncryptedBucketId::Create(plaintext_id, params, ec_cipher.get(), &ctx));
  ASSERT_OK_AND_ASSIGN(auto bucket2,
                       EncryptedBucketId::Create(encrypted_id, params, &ctx));
  EXPECT_THAT(bucket1, Eq(bucket2));
}

TEST(EncryptedBucketIdTest, CreateWithHashingError) {
  EncryptedBucketsParameters params;
  params.set_encrypted_bucket_id_length(14);

  EncryptedBucketsParameters bad_params;
  bad_params.set_encrypted_bucket_id_length(257);

  RlwePlaintextId plaintext_id;
  plaintext_id.set_non_sensitive_id("nsid-test");
  plaintext_id.set_sensitive_id("sid-test");

  ::private_join_and_compute::Context ctx;
  ASSERT_OK_AND_ASSIGN(
      auto ec_cipher,
      ::private_join_and_compute::ECCommutativeCipher::CreateWithNewKey(
          kTestCurveId, ::private_join_and_compute::ECCommutativeCipher::HashType::SHA256));

  EXPECT_THAT(
      EncryptedBucketId::Create(plaintext_id, params, nullptr, &ctx),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("non-null")));
  EXPECT_THAT(
      EncryptedBucketId::Create(plaintext_id, params, ec_cipher.get(), nullptr),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("non-null")));
  EXPECT_THAT(
      EncryptedBucketId::Create("encrypted-id", params, nullptr),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("non-null")));

  EXPECT_THAT(EncryptedBucketId::Create(plaintext_id, bad_params,
                                        ec_cipher.get(), &ctx),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Truncation bit length out of bounds.")));
}

TEST(EncryptedBucketIdTest, ToUint32Error) {
  ASSERT_OK_AND_ASSIGN(auto encrypted_bucket_id,
                       EncryptedBucketId::Create("\xFF\xFF\xFF\xFF\xFF", 40));
  EXPECT_THAT(encrypted_bucket_id.ToUint32(),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Bit length exceeds 32 bits.")));
}

TEST(EncryptedBucketIdTest, ToUint32Success) {
  ASSERT_OK_AND_ASSIGN(auto encrypted_bucket_id,
                       EncryptedBucketId::Create("\xFF\xFF\xFF\xFF", 32));
  ASSERT_OK_AND_ASSIGN(auto x, encrypted_bucket_id.ToUint32());
  EXPECT_EQ(x, (int64_t{1} << 32) - 1);
}

TEST(EncryptedBucketIdTest, EqualsFalse) {
  ASSERT_OK_AND_ASSIGN(auto encrypted_bucket_id1,
                       EncryptedBucketId::Create("abcd", 32));
  ASSERT_OK_AND_ASSIGN(auto encrypted_bucket_id2,
                       EncryptedBucketId::Create("Abcd", 32));
  EXPECT_THAT(encrypted_bucket_id1, Ne(encrypted_bucket_id2));
}

TEST(EncryptedBucketIdTest, EqualsTrue) {
  ASSERT_OK_AND_ASSIGN(auto encrypted_bucket_id1,
                       EncryptedBucketId::Create("abcd", 32));
  ASSERT_OK_AND_ASSIGN(auto encrypted_bucket_id2,
                       EncryptedBucketId::Create("abcd", 32));
  EXPECT_THAT(encrypted_bucket_id1, Eq(encrypted_bucket_id2));
}

}  // namespace
}  // namespace rlwe
}  // namespace private_membership

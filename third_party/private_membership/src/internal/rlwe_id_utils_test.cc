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

#include "third_party/private_membership/src/internal/rlwe_id_utils.h"

#include "third_party/private_membership/src/private_membership.pb.h"
#include "third_party/private_membership/src/internal/constants.h"
#include "third_party/private_membership/src/internal/testing/constants.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/status/status.h"
#include "third_party/shell-encryption/src/testing/protobuf_matchers.h"
#include "third_party/shell-encryption/src/testing/status_matchers.h"
#include "third_party/shell-encryption/src/testing/status_testing.h"

namespace private_membership {
namespace rlwe {
namespace {

using ::rlwe::testing::StatusIs;
using ::testing::HasSubstr;

TEST(RlweIdUtils, ComputeBucketStoredEncryptedIdSuccess) {
  EncryptedBucketsParameters params;
  params.set_encrypted_bucket_id_length(14);

  RlwePlaintextId plaintext_id;
  plaintext_id.set_non_sensitive_id("nsid");
  plaintext_id.set_sensitive_id("sid");

  private_join_and_compute::Context ctx;
  ASSERT_OK_AND_ASSIGN(
      auto ec_cipher,
      private_join_and_compute::ECCommutativeCipher::CreateWithNewKey(
          kTestCurveId, private_join_and_compute::ECCommutativeCipher::HashType::SHA256));

  std::string full_id = HashRlwePlaintextId(plaintext_id);
  ASSERT_OK_AND_ASSIGN(std::string encrypted_id, ec_cipher->Encrypt(full_id));

  ASSERT_OK_AND_ASSIGN(std::string id1,
                       ComputeBucketStoredEncryptedId(plaintext_id, params,
                                                      ec_cipher.get(), &ctx));
  ASSERT_OK_AND_ASSIGN(std::string id2, ComputeBucketStoredEncryptedId(
                                            encrypted_id, params, &ctx));
  EXPECT_EQ(id1, id2);
  EXPECT_GE(id1.length() + params.encrypted_bucket_id_length(),
            kStoredEncryptedIdByteLength);
}

TEST(RlweIdUtils, ComputeBucketStoredEncryptedIdEmpty) {
  EncryptedBucketsParameters empty_id_params;
  empty_id_params.set_encrypted_bucket_id_length(kStoredEncryptedIdByteLength *
                                                 8);

  RlwePlaintextId plaintext_id;
  plaintext_id.set_non_sensitive_id("empty-nsid");
  plaintext_id.set_sensitive_id("empty-sid");

  private_join_and_compute::Context ctx;
  ASSERT_OK_AND_ASSIGN(
      auto ec_cipher,
      private_join_and_compute::ECCommutativeCipher::CreateWithNewKey(
          kTestCurveId, private_join_and_compute::ECCommutativeCipher::HashType::SHA256));

  ASSERT_OK_AND_ASSIGN(std::string id, ComputeBucketStoredEncryptedId(
                                           plaintext_id, empty_id_params,
                                           ec_cipher.get(), &ctx));
  EXPECT_TRUE(id.empty());
}

TEST(RlweIdUtils, ComputeBucketStoredEncryptedIdError) {
  EncryptedBucketsParameters params;
  params.set_encrypted_bucket_id_length(14);

  RlwePlaintextId plaintext_id;
  plaintext_id.set_non_sensitive_id("nsid-test");
  plaintext_id.set_sensitive_id("sid-test");

  private_join_and_compute::Context ctx;
  ASSERT_OK_AND_ASSIGN(
      auto ec_cipher,
      private_join_and_compute::ECCommutativeCipher::CreateWithNewKey(
          kTestCurveId, private_join_and_compute::ECCommutativeCipher::HashType::SHA256));

  EXPECT_THAT(
      ComputeBucketStoredEncryptedId(plaintext_id, params, nullptr, &ctx),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("non-null")));
  EXPECT_THAT(
      ComputeBucketStoredEncryptedId(plaintext_id, params, ec_cipher.get(),
                                     nullptr),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("non-null")));
  EXPECT_THAT(
      ComputeBucketStoredEncryptedId("encrypted-id", params, nullptr),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("non-null")));
}

TEST(RlweIdUtils, EnsureInjectiveInternalProtoNoNonSensitiveId) {
  RlwePlaintextId id1;
  id1.set_sensitive_id("sid1");

  RlwePlaintextId id2;
  id2.set_sensitive_id("sid2");

  EXPECT_EQ(HashRlwePlaintextId(id1), HashRlwePlaintextId(id1));
  EXPECT_EQ(HashRlwePlaintextId(id2), HashRlwePlaintextId(id2));
  EXPECT_NE(HashRlwePlaintextId(id1), HashRlwePlaintextId(id2));
}

TEST(RlweIdUtils, EnsureInjectiveInternalProto) {
  RlwePlaintextId id1;
  id1.set_non_sensitive_id("nsid1");
  id1.set_sensitive_id("sid1");

  RlwePlaintextId id2;
  id2.set_non_sensitive_id("nsid2");
  id2.set_sensitive_id("sid2");

  EXPECT_EQ(HashRlwePlaintextId(id1), HashRlwePlaintextId(id1));
  EXPECT_EQ(HashRlwePlaintextId(id2), HashRlwePlaintextId(id2));
  EXPECT_NE(HashRlwePlaintextId(id1), HashRlwePlaintextId(id2));

  RlwePlaintextId id3;
  id3.set_non_sensitive_id("1");
  id3.set_sensitive_id("23456789111");

  RlwePlaintextId id4;
  id4.set_non_sensitive_id("11234567891");
  id4.set_sensitive_id("1");

  EXPECT_NE(HashRlwePlaintextId(id3), HashRlwePlaintextId(id4));
}

TEST(RlweIdUtils, HashNonsensitiveIdWithSalt) {
  private_join_and_compute::Context ctx;
  std::string nsid1("nsid1");
  std::string nsid2("nsid2");
  ASSERT_OK_AND_ASSIGN(
      std::string hash1a,
      HashNonsensitiveIdWithSalt(nsid1, HashType::TEST_HASH_TYPE, &ctx));
  ASSERT_OK_AND_ASSIGN(
      std::string hash1b,
      HashNonsensitiveIdWithSalt(nsid1, HashType::TEST_HASH_TYPE, &ctx));
  ASSERT_OK_AND_ASSIGN(
      std::string hash2a,
      HashNonsensitiveIdWithSalt(nsid2, HashType::TEST_HASH_TYPE, &ctx));
  ASSERT_OK_AND_ASSIGN(
      std::string hash2b,
      HashNonsensitiveIdWithSalt(nsid2, HashType::TEST_HASH_TYPE, &ctx));
  EXPECT_EQ(hash1a, hash1b);
  EXPECT_EQ(hash2a, hash2b);
  EXPECT_NE(hash1a, hash2a);
}

}  // namespace
}  // namespace rlwe
}  // namespace private_membership

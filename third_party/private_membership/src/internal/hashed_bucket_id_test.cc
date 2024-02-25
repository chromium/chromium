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

#include "third_party/private_membership/src/internal/hashed_bucket_id.h"

#include <string>

#include "third_party/private_membership/src/private_membership.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
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

private_membership::rlwe::PrivateMembershipRlweQuery::HashedBucketId
ApiHashedBucketId(absl::string_view hashed_bucket_id, int bit_length) {
  private_membership::rlwe::PrivateMembershipRlweQuery::HashedBucketId
      api_hashed_bucket_id;
  api_hashed_bucket_id.set_hashed_bucket_id(std::string(hashed_bucket_id));
  api_hashed_bucket_id.set_bit_length(bit_length);
  return api_hashed_bucket_id;
}

TEST(HashedBucketIdTest, CreateError) {
  EXPECT_THAT(HashedBucketId::Create("abcd", 33),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid bit_length.")));
}

TEST(HashedBucketIdTest, CreateSuccess) {
  EXPECT_OK(HashedBucketId::Create("abcd", 32));
}

TEST(HashedBucketIdTest, CreateFromApiProtoError) {
  EXPECT_THAT(HashedBucketId::CreateFromApiProto(ApiHashedBucketId("abcd", 33)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid API HashedBucketId proto.")));
}

TEST(HashedBucketIdTest, CreateFromApiProtoEmpty) {
  EXPECT_OK(HashedBucketId::CreateFromApiProto(ApiHashedBucketId("", 0)));
}

TEST(HashedBucketIdTest, CreateFromApiProtoSuccess) {
  EXPECT_OK(HashedBucketId::CreateFromApiProto(ApiHashedBucketId("abcd", 32)));
}

TEST(HashedBucketIdTest, CreateWithHashingSuccess) {
  HashedBucketsParameters params;
  params.set_hashed_bucket_id_length(10);
  params.set_non_sensitive_id_hash_type(TEST_HASH_TYPE);

  RlwePlaintextId plaintext_id;
  plaintext_id.set_non_sensitive_id("nsid");
  plaintext_id.set_sensitive_id("sid");

  ::private_join_and_compute::Context ctx;

  ASSERT_OK_AND_ASSIGN(auto bucket1,
                       HashedBucketId::Create(plaintext_id, params, &ctx));
  ASSERT_OK_AND_ASSIGN(auto bucket2,
                       HashedBucketId::Create(plaintext_id, params, &ctx));
  EXPECT_THAT(bucket1, Eq(bucket2));
}

TEST(HashedBucketIdTest, CreateWithHashingError) {
  HashedBucketsParameters params;
  params.set_hashed_bucket_id_length(18);
  params.set_non_sensitive_id_hash_type(TEST_HASH_TYPE);

  HashedBucketsParameters bad_params;
  bad_params.set_hashed_bucket_id_length(257);
  bad_params.set_non_sensitive_id_hash_type(TEST_HASH_TYPE);

  RlwePlaintextId plaintext_id;
  plaintext_id.set_non_sensitive_id("nsid-test");
  plaintext_id.set_sensitive_id("sid-test");

  ::private_join_and_compute::Context ctx;

  EXPECT_THAT(
      HashedBucketId::Create(plaintext_id, params, nullptr),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("non-null")));
  EXPECT_THAT(HashedBucketId::Create(plaintext_id, bad_params, &ctx),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Truncation bit length out of bounds.")));
}

TEST(HashedBucketId, CreateEmptyBucketId) {
  HashedBucketsParameters params;
  params.set_hashed_bucket_id_length(0);
  params.set_non_sensitive_id_hash_type(HASH_TYPE_UNDEFINED);

  RlwePlaintextId plaintext_id;
  plaintext_id.set_non_sensitive_id("nsid-empty");
  plaintext_id.set_sensitive_id("sid-empty");

  ::private_join_and_compute::Context ctx;

  ASSERT_OK_AND_ASSIGN(HashedBucketId id1,
                       HashedBucketId::Create(plaintext_id, params, &ctx));
  ASSERT_OK_AND_ASSIGN(HashedBucketId id2, HashedBucketId::Create("", 0));
  EXPECT_THAT(id1, Eq(id2));
}

TEST(HashedBucketIdTest, EqualsFalse) {
  ASSERT_OK_AND_ASSIGN(
      auto hashed_bucket_id1,
      HashedBucketId::CreateFromApiProto(ApiHashedBucketId("abcd", 32)));
  ASSERT_OK_AND_ASSIGN(auto hashed_bucket_id2,
                       HashedBucketId::Create("Abcd", 32));
  EXPECT_THAT(hashed_bucket_id1, Ne(hashed_bucket_id2));
}

TEST(HashedBucketIdTest, EqualsTrue) {
  ASSERT_OK_AND_ASSIGN(
      auto hashed_bucket_id1,
      HashedBucketId::CreateFromApiProto(ApiHashedBucketId("abcd", 32)));
  ASSERT_OK_AND_ASSIGN(auto hashed_bucket_id2,
                       HashedBucketId::Create("abcd", 32));
  EXPECT_THAT(hashed_bucket_id1, Eq(hashed_bucket_id2));
}

}  // namespace
}  // namespace rlwe
}  // namespace private_membership

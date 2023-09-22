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
#include <utility>

#include "third_party/private_membership/src/private_membership.pb.h"
#include "third_party/private_membership/src/internal/rlwe_id_utils.h"
#include "third_party/private_membership/src/internal/utils.h"
#include "absl/strings/string_view.h"
#include "third_party/shell-encryption/src/status_macros.h"

namespace private_membership {
namespace rlwe {

namespace {

bool IsEmpty(absl::string_view hashed_bucket_id, int bit_length) {
  return hashed_bucket_id.empty() && bit_length == 0;
}

}  // namespace

::rlwe::StatusOr<HashedBucketId> HashedBucketId::Create(
    absl::string_view hashed_bucket_id, int bit_length) {
  // Allow "empty" hashed bucket id to be created. This case can occur if the
  // use case doesn't support bucketing by hashed bucket id. Otherwise, the
  // hashed bucket id/bit length pair must be valid.
  if (!IsEmpty(hashed_bucket_id, bit_length) &&
      !IsValid(hashed_bucket_id, bit_length)) {
    return absl::InvalidArgumentError("Invalid bit_length.");
  }
  return HashedBucketId(hashed_bucket_id, bit_length);
}

::rlwe::StatusOr<HashedBucketId> HashedBucketId::Create(
    const RlwePlaintextId& id,
    const private_membership::rlwe::HashedBucketsParameters& params,
    ::private_join_and_compute::Context* ctx) {
  // If the bucket ID length is 0, ignore hash.
  if (params.hashed_bucket_id_length() == 0) {
    return Create("", /*bit_length=*/0);
  }
  RLWE_ASSIGN_OR_RETURN(
      std::string hashed_non_sensitive_id,
      rlwe::HashNonsensitiveIdWithSalt(
          id.non_sensitive_id(), params.non_sensitive_id_hash_type(), ctx));
  auto hashed_bucket_id =
      rlwe::Truncate(hashed_non_sensitive_id, params.hashed_bucket_id_length());
  if (!hashed_bucket_id.ok()) {
    return hashed_bucket_id.status();
  }
  return Create(std::move(hashed_bucket_id).value(),
                params.hashed_bucket_id_length());
}

::rlwe::StatusOr<HashedBucketId> HashedBucketId::CreateFromApiProto(
    const private_membership::rlwe::PrivateMembershipRlweQuery::HashedBucketId&
        api_hashed_bucket_id) {
  // Allow "empty" hashed bucket id to be created. This case can occur if the
  // use case doesn't support bucketing by hashed bucket id. Otherwise, the
  // hashed bucket id/bit length pair must be valid.
  if (!IsEmpty(api_hashed_bucket_id.hashed_bucket_id(),
               api_hashed_bucket_id.bit_length()) &&
      !IsValid(api_hashed_bucket_id.hashed_bucket_id(),
               api_hashed_bucket_id.bit_length())) {
    return absl::InvalidArgumentError("Invalid API HashedBucketId proto.");
  }
  return HashedBucketId(api_hashed_bucket_id.hashed_bucket_id(),
                        api_hashed_bucket_id.bit_length());
}

private_membership::rlwe::PrivateMembershipRlweQuery::HashedBucketId
HashedBucketId::ToApiProto() const {
  private_membership::rlwe::PrivateMembershipRlweQuery::HashedBucketId
      api_proto;
  api_proto.set_hashed_bucket_id(hashed_bucket_id_bytes_);
  api_proto.set_bit_length(bit_length_);
  return api_proto;
}
//
::rlwe::StatusOr<uint32_t> HashedBucketId::ToUint32() const {
  if (bit_length_ > 32) {
    return absl::InternalError("Bit length exceeds 32 bits.");
  }
  return TruncateAsUint32(hashed_bucket_id_bytes_, bit_length_);
}

std::string HashedBucketId::DebugString() const {
  return ToApiProto().DebugString();
}

HashedBucketId::HashedBucketId(absl::string_view hashed_bucket_id_bytes,
                               int bit_length)
    : hashed_bucket_id_bytes_(hashed_bucket_id_bytes),
      bit_length_(bit_length) {}

}  // namespace rlwe
}  // namespace private_membership

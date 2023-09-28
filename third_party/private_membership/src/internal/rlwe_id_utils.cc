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

#include <string>
#include <utility>

#include "third_party/private_membership/src/internal/crypto_utils.h"
#include "third_party/private_membership/src/private_membership.pb.h"
#include "third_party/private_membership/src/internal/constants.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "third_party/shell-encryption/src/status_macros.h"

namespace private_membership {
namespace rlwe {

::rlwe::StatusOr<std::string> ComputeBucketStoredEncryptedId(
    const RlwePlaintextId& id, const EncryptedBucketsParameters& params,
    ::private_join_and_compute::ECCommutativeCipher* ec_cipher, ::private_join_and_compute::Context* ctx) {
  if (ec_cipher == nullptr || ctx == nullptr) {
    return absl::InvalidArgumentError(
        "ECCipher and Context should both be non-null.");
  }
  std::string full_id = rlwe::HashRlwePlaintextId(id);
  auto encrypted_id = ec_cipher->Encrypt(full_id);
  if (!encrypted_id.ok()) {
    return absl::InternalError(encrypted_id.status().message());
  }
  return ComputeBucketStoredEncryptedId(std::move(encrypted_id).value(), params,
                                        ctx);
}

::rlwe::StatusOr<std::string> ComputeBucketStoredEncryptedId(
    const absl::string_view encrypted_id,
    const EncryptedBucketsParameters& params, ::private_join_and_compute::Context* ctx) {
  if (ctx == nullptr) {
    return absl::InvalidArgumentError("Context should be non-null.");
  }
  const std::string hashed_encrypted_id = HashEncryptedId(encrypted_id, ctx);

  switch (params.sensitive_id_hash_type()) {
    case ENCRYPTED_BUCKET_HASH_TYPE_UNDEFINED: {
      return absl::InvalidArgumentError(
          "Sensitive id hash type must be defined.");
    }
    case ENCRYPTED_BUCKET_TEST_HASH_TYPE:
    case SHA256_NON_SENSITIVE_AND_SENSITIVE_ID: {
      // Find the first byte that is not used by the bucket ID.
      int start_byte = (params.encrypted_bucket_id_length() - 1) / 8 + 1;

      // Ensure that the total of the bytes stored in the bucket plus the length
      // of the bucket ID is at least kBucketStoredEncryptedIdByteLength. Since
      // the ID within a bucket needs to be represented at byte-level
      // granularity, pad up the remainder of a byte if needed.
      int byte_length = kStoredEncryptedIdByteLength -
                        (params.encrypted_bucket_id_length() / 8);
      if (byte_length < 0) {
        byte_length = 0;
      }
      std::string stored_encrypted_id(hashed_encrypted_id, start_byte,
                                      byte_length);
      return stored_encrypted_id;
    }
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unknown sensitive id hash type: ", params.sensitive_id_hash_type()));
  }
}

std::string HashRlwePlaintextId(const RlwePlaintextId& id) {
  // For backward compatibility. We can show that if non_sensitive_id length is
  // 0, it is okay to concatenate without delimiters to make it injective.
  if (id.non_sensitive_id().empty()) {
    return absl::StrCat(0, id.sensitive_id().length(), id.sensitive_id());
  }
  static constexpr char delimiter[] = "/";
  // Add delimiters in between to make the function injective.
  return absl::StrCat(id.non_sensitive_id().length(), delimiter,
                      id.non_sensitive_id(), delimiter,
                      id.sensitive_id().length(), delimiter, id.sensitive_id());
}

::rlwe::StatusOr<std::string> HashNonsensitiveIdWithSalt(
    absl::string_view nsid, HashType hash_type, ::private_join_and_compute::Context* ctx) {
  switch (hash_type) {
    case HashType::TEST_HASH_TYPE:
    case HashType::SHA256: {
      if (ctx == nullptr) {
        return absl::InvalidArgumentError("Context must be non-null.");
      }
      // Randomly generated salt forcing adversaries to re-compute rainbow
      // tables.
      static constexpr unsigned char kHashedBucketIdSalt[] = {
          0xD6, 0x50, 0x82, 0x81, 0x82, 0xD9, 0x99, 0x11, 0x61, 0xE6, 0x7D,
          0xB2, 0x91, 0x72, 0xE4, 0x05, 0x3E, 0x4A, 0xE8, 0x54, 0x0D, 0xFF,
          0xB7, 0x8F, 0x61, 0x08, 0x0D, 0x96, 0x4D, 0x8F, 0x58, 0xFE};
      return ctx->Sha256String(absl::StrCat(
          std::string(reinterpret_cast<const char*>(kHashedBucketIdSalt), 32),
          nsid));
      break;
    }
    default:
      return absl::InvalidArgumentError("Invalid hash type.");
  }
}

}  // namespace rlwe
}  // namespace private_membership

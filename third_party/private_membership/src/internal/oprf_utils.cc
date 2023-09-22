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

#include <string>
#include <utility>

#include "third_party/shell-encryption/src/status_macros.h"

namespace private_membership {

::rlwe::StatusOr<DoublyEncryptedId> ReEncryptId(
    absl::string_view encrypted_id,
    ::private_join_and_compute::ECCommutativeCipher* ec_cipher) {
  DoublyEncryptedId doubly_encrypted_id;

  doubly_encrypted_id.set_queried_encrypted_id(std::string(encrypted_id));

  auto reencrypted_id =
      ec_cipher->ReEncrypt(std::string(encrypted_id)); // NOLINT
  if (!reencrypted_id.ok()) {
    return reencrypted_id.status();
  }

  doubly_encrypted_id.set_doubly_encrypted_id(
      std::string(std::move(reencrypted_id).value()));

  return doubly_encrypted_id;
}

}  // namespace private_membership

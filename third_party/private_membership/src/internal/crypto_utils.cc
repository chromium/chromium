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

#include "third_party/private_membership/src/internal/crypto_utils.h"

#include <string>

#include "third_party/private_membership/src/internal/aes_ctr_256_with_fixed_iv.h"
#include "third_party/private_membership/src/internal/id_utils.h"
#include "third_party/shell-encryption/src/status_macros.h"

namespace private_membership {
namespace {

std::string LengthToBytes(uint32_t length) {
  uint8_t ret[4];
  ret[0] = length & 0xFF;
  ret[1] = (length >> 8) & 0xFF;
  ret[2] = (length >> 16) & 0xFF;
  ret[3] = (length >> 24) & 0xFF;
  return std::string(ret, ret + 4);
}

::rlwe::StatusOr<uint32_t> BytesToLength(absl::string_view bytes) {
  if (bytes.size() != 4) {
    return absl::InvalidArgumentError("Invalid byte size of length encoding.");
  }
  uint32_t ret = 0;
  ret += static_cast<uint8_t>(bytes[0]);
  ret += (static_cast<uint8_t>(bytes[1]) << 8);
  ret += (static_cast<uint8_t>(bytes[2]) << 16);
  ret += (static_cast<uint8_t>(bytes[3]) << 24);
  return ret;
}

}  // namespace

::rlwe::StatusOr<std::string> Pad(absl::string_view bytes,
                                  int max_byte_length) {
  if (max_byte_length < bytes.length()) {
    return absl::InvalidArgumentError(
        "max_byte_length smaller than the input bytes length.");
  }
  // Prepend byte length of value into the first four bytes. This limits the
  // maximum length of the value to at most 2^32 bytes or ~4.3 gigabytes.
  std::string length_in_byte = LengthToBytes(bytes.length());
  std::string padded_bytes =
      std::string(bytes).append(max_byte_length - bytes.length(), 0);
  return absl::StrCat(length_in_byte, padded_bytes);
}

::rlwe::StatusOr<std::string> Unpad(absl::string_view bytes) {
  if (bytes.length() < 4) {
    return absl::InvalidArgumentError("Invalid bytes does not encode length.");
  }
  RLWE_ASSIGN_OR_RETURN(uint32_t bytes_length,
                        BytesToLength(bytes.substr(0, 4)));
  if (bytes_length + 4 > bytes.length()) {
    return absl::InvalidArgumentError("Incorrect bytes length.");
  }
  return std::string(bytes.substr(4, bytes_length));
}

std::string HashEncryptedId(absl::string_view encrypted_id,
                            ::private_join_and_compute::Context* ctx) {
  // Salt used in SHA256 hash function for creating sensitive matching
  // identifier. Randomly generated salt forcing adversaries to re-compute
  // rainbow tables.
  static constexpr unsigned char kSensitiveIdHashSalt[] = {
      0x3C, 0xD1, 0xF3, 0x69, 0x2B, 0x57, 0x40, 0xEA, 0xD8, 0xE4, 0xF4,
      0x4A, 0xB2, 0x5F, 0x7B, 0xAD, 0xC8, 0x10, 0xAA, 0x3D, 0x4C, 0x6E,
      0xCA, 0x57, 0x78, 0x5C, 0x5A, 0xED, 0x06, 0x81, 0x14, 0x7C};
  return ctx->Sha256String(absl::StrCat(
      std::string(reinterpret_cast<const char*>(kSensitiveIdHashSalt), 32),
      encrypted_id));
}

::rlwe::StatusOr<std::string> GetValueEncryptionKey(
    absl::string_view encrypted_id, ::private_join_and_compute::Context* ctx) {
  // Salt user in SHA256 hash function to generate value encryption key.
  // Randomly generated salt forcing adversaries to re-compute rainbow tables.
  static constexpr unsigned char kValueEncryptionKeyHashSalt[] = {
      0x89, 0xC3, 0x67, 0xA7, 0x8A, 0x68, 0x2B, 0xE8, 0xC6, 0xB2, 0x22,
      0xB7, 0xE0, 0xB7, 0x4A, 0x37, 0x63, 0x8F, 0x10, 0x79, 0x98, 0x91,
      0x31, 0x94, 0x44, 0x03, 0xB6, 0x76, 0x8F, 0x70, 0xEB, 0xBF};
  // AES key length in bytes.
  static constexpr int kRequiredAesKeyLength = 32;
  return PadOrTruncate(
      ctx->Sha256String(absl::StrCat(
          std::string(
              reinterpret_cast<const char*>(kValueEncryptionKeyHashSalt), 32),
          encrypted_id)),
      kRequiredAesKeyLength);
}

::rlwe::StatusOr<std::string> EncryptValue(absl::string_view encrypted_id,
                                           absl::string_view value,
                                           uint32_t max_value_byte_length,
                                           ::private_join_and_compute::Context* ctx) {
  if (value.length() > max_value_byte_length) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Length of value ", value.length(), " larger than maximum value byte ",
        "length ", max_value_byte_length, "."));
  }
  RLWE_ASSIGN_OR_RETURN(auto value_encryption_key,
                        GetValueEncryptionKey(encrypted_id, ctx));
  RLWE_ASSIGN_OR_RETURN(auto aes_ctr,
                        AesCtr256WithFixedIV::Create(value_encryption_key));
  RLWE_ASSIGN_OR_RETURN(std::string plaintext,
                        Pad(value, max_value_byte_length));
  return aes_ctr->Encrypt(plaintext);
}

::rlwe::StatusOr<std::string> DecryptValue(absl::string_view encrypted_id,
                                           absl::string_view encrypted_value,
                                           ::private_join_and_compute::Context* ctx) {
  RLWE_ASSIGN_OR_RETURN(auto value_encryption_key,
                        GetValueEncryptionKey(encrypted_id, ctx));
  return DecryptValueWithKey(value_encryption_key, encrypted_value, ctx);
}

::rlwe::StatusOr<std::string> DecryptValueWithKey(
    absl::string_view encryption_key, absl::string_view encrypted_value,
    ::private_join_and_compute::Context* ctx) {
  RLWE_ASSIGN_OR_RETURN(auto aes_ctr,
                        AesCtr256WithFixedIV::Create(encryption_key));
  RLWE_ASSIGN_OR_RETURN(std::string plaintext,
                        aes_ctr->Decrypt(encrypted_value));
  return Unpad(plaintext);
}

}  // namespace private_membership

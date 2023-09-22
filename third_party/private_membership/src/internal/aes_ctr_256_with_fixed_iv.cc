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

#include "third_party/private_membership/src/internal/aes_ctr_256_with_fixed_iv.h"

#include <memory>
#include <string>
#include <vector>

#include <openssl/err.h>
#include <openssl/evp.h>

namespace private_membership {

namespace {

absl::string_view EnsureNotNull(absl::string_view str) {
  if (str.empty() && str.data() == nullptr) {
    return absl::string_view("");
  }
  return str;
}

}  // namespace

::rlwe::StatusOr<std::unique_ptr<AesCtr256WithFixedIV>>
AesCtr256WithFixedIV::Create(absl::string_view key) {
  if (key.size() != kKeySize) {
    return absl::InvalidArgumentError("Key size is invalid.");
  }
  return absl::WrapUnique<AesCtr256WithFixedIV>(new AesCtr256WithFixedIV(key));
}

AesCtr256WithFixedIV::AesCtr256WithFixedIV(absl::string_view key)
    : key_(EnsureNotNull(key)), iv_(kIVSize, 0) {}

::rlwe::StatusOr<std::string> AesCtr256WithFixedIV::Encrypt(
    absl::string_view plaintext) const {
  plaintext = EnsureNotNull(plaintext);

  bssl::UniquePtr<EVP_CIPHER_CTX> ctx(EVP_CIPHER_CTX_new());
  if (ctx.get() == nullptr) {
    return absl::InternalError("Could not initialize EVP_CIPHER_CTX.");
  }

  int ret = EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_ctr(),
                               /*impl=*/nullptr,
                               reinterpret_cast<const uint8_t*>(key_.data()),
                               reinterpret_cast<const uint8_t*>(iv_.data()));
  if (ret != 1) {
    return absl::InternalError("Could not initialize encryption.");
  }

  // Allocates 1 byte more than necessary because it may potentially access
  // &ciphertext[plaintext.size()] causing vector range check error.
  std::vector<uint8_t> ciphertext(plaintext.size() + 1);
  int len;
  ret = EVP_EncryptUpdate(ctx.get(), &ciphertext[0], &len,
                          reinterpret_cast<const uint8_t*>(plaintext.data()),
                          plaintext.size());
  if (ret != 1) {
    return absl::InternalError("Encryption failed.");
  }

  if (len != plaintext.size()) {
    return absl::InternalError("Ciphertext is incorrect size.");
  }
  return std::string(reinterpret_cast<const char*>(&ciphertext[0]),
                     plaintext.size());
}

::rlwe::StatusOr<std::string> AesCtr256WithFixedIV::Decrypt(
    absl::string_view ciphertext) const {
  if (ciphertext.data() == nullptr) {
    return absl::InternalError("Invalid null ciphertext.");
  }

  bssl::UniquePtr<EVP_CIPHER_CTX> ctx(EVP_CIPHER_CTX_new());
  if (ctx.get() == nullptr) {
    return absl::InternalError("Could not initialize EVP_CIPHER_CTX.");
  }

  int ret = EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_ctr(),
                               /*impl=*/nullptr,
                               reinterpret_cast<const uint8_t*>(key_.data()),
                               reinterpret_cast<const uint8_t*>(iv_.data()));
  if (ret != 1) {
    return absl::InternalError("Could not initialize decryption.");
  }

  // Allocates 1 byte more than necessary because it may potentially access
  // &plaintext[ciphertext.size()] causing vector range check error.
  std::vector<uint8_t> plaintext(ciphertext.size() + 1);
  int len;
  ret = EVP_DecryptUpdate(ctx.get(), &plaintext[0], &len,
                          reinterpret_cast<const uint8_t*>(ciphertext.data()),
                          ciphertext.size());
  if (ret != 1) {
    return absl::InternalError("Decryption failed.");
  }

  if (len != ciphertext.size()) {
    return absl::InternalError("Plaintext is incorrect size.");
  }
  return std::string(reinterpret_cast<const char*>(&plaintext[0]),
                     ciphertext.size());
}

}  // namespace private_membership

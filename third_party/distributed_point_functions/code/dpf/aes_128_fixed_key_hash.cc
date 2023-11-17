/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dpf/aes_128_fixed_key_hash.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "openssl/err.h"

namespace distributed_point_functions {

Aes128FixedKeyHash::Aes128FixedKeyHash(
    bssl::UniquePtr<EVP_CIPHER_CTX> cipher_ctx, absl::uint128 key)
    : cipher_ctx_(std::move(cipher_ctx)), key_(key) {}

absl::StatusOr<Aes128FixedKeyHash> Aes128FixedKeyHash::Create(
    absl::uint128 key) {
  bssl::UniquePtr<EVP_CIPHER_CTX> cipher_ctx(EVP_CIPHER_CTX_new());
  if (!cipher_ctx) {
    return absl::InternalError("Failed to allocate AES context");
  }
  // Set up the OpenSSL encryption context. We want to evaluate the PRG in
  // parallel on many seeds (see class comment in pseudorandom_generator.h), so
  // we're using ECB mode here to achieve that. This batched evaluation is not
  // to be confused with encryption of an array, for which ECB would be
  // insecure.
  int openssl_status =
      EVP_EncryptInit_ex(cipher_ctx.get(), EVP_aes_128_ecb(), nullptr,
                         reinterpret_cast<const uint8_t*>(&key), nullptr);
  if (openssl_status != 1) {
    return absl::InternalError("Failed to set up AES context");
  }
  return Aes128FixedKeyHash(std::move(cipher_ctx), key);
}

absl::Status Aes128FixedKeyHash::Evaluate(absl::Span<const absl::uint128> in,
                                          absl::Span<absl::uint128> out) const {
  if (in.size() != out.size()) {
    return absl::InvalidArgumentError("Input and output sizes don't match");
  }
  if (in.empty()) {
    // Nothing to do.
    return absl::OkStatus();
  }

  // Compute orthomorphism sigma for each element in `in`, `kBatchSize` elements
  // at a time.
  auto in_size = static_cast<int64_t>(in.size());
  std::array<absl::uint128, kBatchSize> sigma_in;
  for (int64_t start_block = 0; start_block < in_size;
       start_block += kBatchSize) {
    int64_t batch_size = std::min<int64_t>(in_size - start_block, kBatchSize);
    for (int i = 0; i < batch_size; ++i) {
      sigma_in[i] =
          absl::MakeUint128(absl::Uint128High64(in[start_block + i]) ^
                                absl::Uint128Low64(in[start_block + i]),
                            absl::Uint128High64(in[start_block + i]));
    }

    // We use EVP_Cipher here instead of EVP_EncryptUpdate, since it doesn't
    // mutate the context in ECB mode, and so this call is thread-safe.
    int openssl_status = EVP_Cipher(
        cipher_ctx_.get(), reinterpret_cast<uint8_t*>(out.data() + start_block),
        reinterpret_cast<const uint8_t*>(sigma_in.data()),
        static_cast<int>(batch_size * sizeof(absl::uint128)));
    if (openssl_status != 1) {
      char buf[256];
      ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
      return absl::InternalError(
          absl::StrCat("AES encryption failed: ", std::string(buf)));
    }
    for (int64_t i = 0; i < batch_size; ++i) {
      out[start_block + i] ^= sigma_in[i];
    }
  }
  return absl::OkStatus();
}

}  // namespace distributed_point_functions

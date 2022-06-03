// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "prng/chacha_prng_util.h"

#include <cstdint>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include <openssl/chacha.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include "status_macros.h"

namespace rlwe {
namespace internal {

absl::Status ChaChaPrngResalt(absl::string_view key, int buffer_size,
                              int* salt_counter, int* position_in_buffer,
                              std::vector<Uint8>* buffer) {
  buffer->assign(buffer_size, 0);

  // Following https://tools.ietf.org/html/rfc7539, Sec 2.3, we create the
  // nonce as a kChaChaNonceSize (=12) bytes string, where the 4 first
  // bytes are fixed, and the next 8 bytes correspond to the counter.
  std::string nonce = "salt00000000";
  if (nonce.size() != kChaChaNonceSize) {
    return absl::InternalError("The salt length is incorrect.");
  }
  Uint64 counter = static_cast<Uint64>(*salt_counter);
  for (int i = 0; i < 8; i++) {
    nonce[4 + i] = counter & 0xFF;
    counter >>= 8;
  }

  // We call the CRYPTO_chacha_20() function from OpenSSL. Note that the last
  // parameter is a *block* counter. The salt counter needs instead to be
  // included in the nonce.
  CRYPTO_chacha_20(buffer->data(), buffer->data(), buffer->size(),
                   reinterpret_cast<const Uint8*>(key.data()),
                   reinterpret_cast<const Uint8*>(nonce.data()),
                   /* counter = */ 0);

  ++(*salt_counter);
  *position_in_buffer = 0;
  return absl::OkStatus();
}

rlwe::StatusOr<std::string> ChaChaPrngGenerateKey() {
  std::unique_ptr<Uint8[]> buf(new Uint8[kChaChaKeyBytesSize]);
  // BoringSSL documentation says that it always returns 1; while
  // OpenSSL documentation says that it returns 1 on success, 0 otherwise. Check
  // for an error just in case.
  if (RAND_bytes(buf.get(), kChaChaKeyBytesSize) == 0) {
    return absl::InternalError("Internal error generating random PRNG key.");
  }
  return std::string(reinterpret_cast<const char*>(buf.get()),
                     kChaChaKeyBytesSize);
}

rlwe::StatusOr<Uint8> ChaChaPrngRand8(absl::string_view key,
                                      int* position_in_buffer,
                                      int* salt_counter,
                                      std::vector<Uint8>* buffer) {
  Uint8 rand;
  if (*position_in_buffer >= buffer->size()) {
    RLWE_RETURN_IF_ERROR(ChaChaPrngResalt(key, kChaChaOutputBytes, salt_counter,
                                          position_in_buffer, buffer));
  }
  rand = buffer->at(*position_in_buffer);
  ++(*position_in_buffer);
  return rand;
}

rlwe::StatusOr<Uint64> ChaChaPrngRand64(absl::string_view key,
                                        int* position_in_buffer,
                                        int* salt_counter,
                                        std::vector<Uint8>* buffer) {
  Uint64 rand64 = 0;
  for (int i = 0; i < 8; ++i) {
    RLWE_ASSIGN_OR_RETURN(Uint8 rand8, ChaChaPrngRand8(key, position_in_buffer,
                                                       salt_counter, buffer));
    rand64 += Uint64{rand8} << (8 * i);
  }
  return rand64;
}

}  // namespace internal
}  // namespace rlwe

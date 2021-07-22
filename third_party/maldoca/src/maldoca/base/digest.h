// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/strings/escaping.h"
#include "openssl/md5.h"
#include "openssl/sha.h"

namespace maldoca {

// Compute SHA1 of input
inline std::string Sha1Hash(absl::string_view input) {
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(input.data()), input.size(),
       hash);
  return {reinterpret_cast<const char*>(&hash[0]), SHA_DIGEST_LENGTH};
}

// Returns the sha256 digest string for input data.
inline std::string Sha256Digest(absl::string_view data) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         hash);
  return {reinterpret_cast<const char*>(&hash[0]), SHA256_DIGEST_LENGTH};
}

// Returns the sha256 hex string for input data.
inline std::string Sha256HexString(absl::string_view data) {
  auto digest = Sha256Digest(data);
  return absl::BytesToHexString(digest);
}

inline std::string Md5Digest(absl::string_view data) {
  unsigned char hash[MD5_DIGEST_LENGTH];
  MD5(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash);
  return {reinterpret_cast<const char*>(&hash[0]), MD5_DIGEST_LENGTH};
}

inline std::string Md5HexString(absl::string_view data) {
  auto digest = Md5Digest(data);
  return absl::BytesToHexString(digest);
}

}  // namespace maldoca

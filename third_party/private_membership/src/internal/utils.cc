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

#include "third_party/private_membership/src/internal/utils.h"

#include <string>

#include "third_party/shell-encryption/src/status_macros.h"

namespace private_membership {
namespace rlwe {

::rlwe::StatusOr<std::string> Truncate(absl::string_view in, int bit_length) {
  if (bit_length < 0 || bit_length > in.size() * 8) {
    return absl::InvalidArgumentError("Truncation bit length out of bounds.");
  }
  if (bit_length == 0) {
    return std::string();
  }
  int ceiled_byte_length = (bit_length - 1) / 8 + 1;
  std::string res(in.begin(), in.begin() + ceiled_byte_length);
  if (int remainder_bit_count = bit_length % 8; remainder_bit_count > 0) {
    int mask = (1 << remainder_bit_count) - 1;
    res[res.size() - 1] &= (mask << (8 - remainder_bit_count));
  }
  return res;
}

::rlwe::StatusOr<uint32_t> TruncateAsUint32(absl::string_view in,
                                            int bit_length) {
  if (bit_length <= 0 || bit_length > in.size() * 8) {
    return absl::InvalidArgumentError("Truncation bit length out of bounds.");
  }
  if (bit_length > 32) {
    return absl::InvalidArgumentError("Input bit length larger than 32 bits.");
  }
  std::string bytes = std::string(in);
  bytes.resize(4);
  uint32_t res = (static_cast<unsigned char>(bytes[0]) << 24) |
                 (static_cast<unsigned char>(bytes[1]) << 16) |
                 (static_cast<unsigned char>(bytes[2]) << 8) |
                 static_cast<unsigned char>(bytes[3]);
  return res >> (32 - bit_length);
}

bool IsValid(absl::string_view in, int bit_length) {
  auto status_or_truncated_bytes = Truncate(in, bit_length);
  if (!status_or_truncated_bytes.ok()) {
    return false;
  }
  return (status_or_truncated_bytes.value() == in);
}

}  // namespace rlwe
}  // namespace private_membership

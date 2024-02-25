// Copyright 2021 Google LLC
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

#include "dpf/int_mod_n.h"

#include <cmath>
#include <string>

#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

namespace distributed_point_functions {

namespace dpf_internal {

double IntModNBase::GetSecurityLevel(int num_samples, absl::uint128 modulus) {
  return 128 + 3 -
         (std::log2(static_cast<double>(modulus)) +
          std::log2(static_cast<double>(num_samples)) +
          std::log2(static_cast<double>(num_samples + 1)));
}

absl::Status IntModNBase::CheckParameters(int num_samples,
                                          int base_integer_bitsize,
                                          absl::uint128 modulus,
                                          double security_parameter) {
  if (num_samples <= 0) {
    return absl::InvalidArgumentError("num_samples must be positive");
  }
  if (base_integer_bitsize <= 0) {
    return absl::InvalidArgumentError("base_integer_bitsize must be positive");
  }
  if (base_integer_bitsize > 128) {
    return absl::InvalidArgumentError(
        "base_integer_bitsize must be at most 128");
  }
  if (base_integer_bitsize < 128 &&
      (absl::uint128{1} << base_integer_bitsize) < modulus) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "kModulus %d out of range for base_integer_bitsize = %d", modulus,
        base_integer_bitsize));
  }

  // Compute the level of security that we will get, and fail if it is
  // insufficient.
  const double sigma = GetSecurityLevel(num_samples, modulus);
  if (security_parameter > sigma) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "For num_samples = %d and kModulus = %d this approach can only "
        "provide "
        "%f bits of statistical security. You can try calling this function "
        "several times with smaller values of num_samples.",
        num_samples, modulus, sigma));
  }
  return absl::OkStatus();
}

absl::StatusOr<int> IntModNBase::GetNumBytesRequired(
    int num_samples, int base_integer_bitsize, absl::uint128 modulus,
    double security_parameter) {
  absl::Status status = CheckParameters(num_samples, base_integer_bitsize,
                                        modulus, security_parameter);
  if (!status.ok()) {
    return status;
  }

  const int base_integer_bytes = ((base_integer_bitsize + 7) / 8);
  // We start the sampling by requiring a 128-bit (16 bytes) block, see
  // function `SampleFromBytes`.
  return 16 + base_integer_bytes * (num_samples - 1);
}

}  // namespace dpf_internal

}  // namespace distributed_point_functions

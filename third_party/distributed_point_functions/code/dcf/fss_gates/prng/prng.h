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

#ifndef DISTRIBUTED_POINT_FUNCTIONS_PRNG_PRNG_H_
#define DISTRIBUTED_POINT_FUNCTIONS_PRNG_PRNG_H_

#include "absl/numeric/int128.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "dpf/status_macros.h"

namespace distributed_point_functions {

// An interface for a secure pseudo-random number generator.
class SecurePrng {
 public:
  virtual absl::StatusOr<uint8_t> Rand8() = 0;
  virtual absl::StatusOr<uint64_t> Rand64() = 0;
  virtual absl::StatusOr<absl::uint128> Rand128() = 0;
  virtual ~SecurePrng() = default;
  static absl::StatusOr<std::unique_ptr<SecurePrng>> Create(
      absl::string_view seed);
  static absl::StatusOr<std::string> GenerateSeed();
  static int SeedLength();
};

}  // namespace distributed_point_functions

#endif  // DISTRIBUTED_POINT_FUNCTIONS_PRNG_PRNG_H_

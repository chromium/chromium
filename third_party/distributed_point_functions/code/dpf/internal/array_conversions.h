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

#ifndef DISTRIBUTED_POINT_FUNCTIONS_DPF_INTERNAL_ARRAY_CONVERSIONS_H_
#define DISTRIBUTED_POINT_FUNCTIONS_DPF_INTERNAL_ARRAY_CONVERSIONS_H_

#include <array>
#include <type_traits>

#include "absl/base/casts.h"
#include "absl/numeric/int128.h"

namespace distributed_point_functions {
namespace dpf_internal {

// Computes the number of unsigned integers of type T that fit into an
// absl::uint128. Results in compile-time errors if T is not an unsigned
// integer, or if it doesn't have a positive size that divides
// sizeof(absl::uint128).
template <typename T>
constexpr size_t ElementsPerBlock() {
  static_assert(
      std::is_same<T, absl::uint128>::value | std::is_unsigned<T>::value,
      "Uint128ToArray<T> may only be used with unsigned types T");
  static_assert(sizeof(T) <= sizeof(absl::uint128),
                "sizeof(T) may not be larger than sizeof(absl::uint128)");
  static_assert(sizeof(T) != 0, "sizeof(T) must be positive");
  static_assert(sizeof(absl::uint128) % sizeof(T) == 0,
                "sizeof(T) must divide sizeof(absl::uint128)");
  return sizeof(absl::uint128) / sizeof(T);
}

// Converts a given 128 bit block to a std::array of unsigned integers in
// little-endian order, i.e., the first item in the returned array corresponds
// to the lowest-order bits in `in`.
template <typename T>
std::array<T, ElementsPerBlock<T>()> Uint128ToArray(absl::uint128 in) {
#ifdef ABSL_IS_LITTLE_ENDIAN
  return absl::bit_cast<std::array<T, ElementsPerBlock<T>()>>(in);
#else
  std::array<T, ElementsPerBlock<T>()> result;
  for (int i = 0; i < ElementsPerBlock<T>(); ++i) {
    // static_cast takes the result modulo 2^element_bitsize, so this is safe.
    result[i] = static_cast<T>(in >> (i * sizeof(T) * 8));
  }
  return result;
#endif
}

// Assembles a std::array of unsigned integers into a single absl::uint128
// block. The first item in `in` will be placed in the lowest-order bits of the
// uint128.
template <typename T>
absl::uint128 ArrayToUint128(const std::array<T, ElementsPerBlock<T>()> &in) {
#ifdef ABSL_IS_LITTLE_ENDIAN
  return absl::bit_cast<absl::uint128>(in);
#else
  absl::uint128 result = 0;
  for (int i = 0; i < ElementsPerBlock<T>(); i++) {
    result |= absl::uint128{in[i]} << (i * sizeof(T) * 8);
  }
  return result;
#endif
}

}  // namespace dpf_internal
}  // namespace distributed_point_functions

#endif  // DISTRIBUTED_POINT_FUNCTIONS_DPF_INTERNAL_ARRAY_CONVERSIONS_H_

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

#include "dpf/internal/value_type_helpers.h"

#include <stdint.h>

#include <cmath>
#include <string>

#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "dpf/distributed_point_function.pb.h"
#include "dpf/int_mod_n.h"
#include "dpf/status_macros.h"

namespace distributed_point_functions {
namespace dpf_internal {

absl::StatusOr<bool> ValueTypesAreEqual(const ValueType& lhs,
                                        const ValueType& rhs) {
  if (lhs.type_case() == ValueType::TypeCase::TYPE_NOT_SET ||
      rhs.type_case() == ValueType::TypeCase::TYPE_NOT_SET) {
    return absl::InvalidArgumentError(
        "Both arguments must be valid ValueTypes");
  } else if (lhs.type_case() == ValueType::kInteger &&
             rhs.type_case() == ValueType::kInteger) {
    return lhs.integer().bitsize() == rhs.integer().bitsize();
  } else if (lhs.type_case() == ValueType::kTuple &&
             rhs.type_case() == ValueType::kTuple &&
             lhs.tuple().elements_size() == rhs.tuple().elements_size()) {
    bool result = true;
    for (int i = 0; i < static_cast<int>(lhs.tuple().elements_size()); ++i) {
      DPF_ASSIGN_OR_RETURN(
          bool element_result,
          ValueTypesAreEqual(lhs.tuple().elements(i), rhs.tuple().elements(i)));
      result &= element_result;
    }
    return result;
  } else if (lhs.type_case() == ValueType::kIntModN &&
             rhs.type_case() == ValueType::kIntModN) {
    const Value::Integer &lhs_modulus = lhs.int_mod_n().modulus(),
                         &rhs_modulus = rhs.int_mod_n().modulus();
    DPF_ASSIGN_OR_RETURN(absl::uint128 lhs_modulus_128,
                         ValueIntegerToUint128(lhs_modulus));
    DPF_ASSIGN_OR_RETURN(absl::uint128 rhs_modulus_128,
                         ValueIntegerToUint128(rhs_modulus));
    return lhs.int_mod_n().base_integer().bitsize() ==
               rhs.int_mod_n().base_integer().bitsize() &&
           lhs_modulus_128 == rhs_modulus_128;
  } else if (lhs.type_case() == ValueType::kXorWrapper &&
             rhs.type_case() == ValueType::kXorWrapper) {
    return lhs.xor_wrapper().bitsize() == rhs.xor_wrapper().bitsize();
  }
  return false;
}

absl::StatusOr<int> BitsNeeded(const ValueType& value_type,
                               double security_parameter) {
  if (value_type.type_case() == ValueType::kInteger) {
    return value_type.integer().bitsize();
  } else if (value_type.type_case() == ValueType::kTuple) {
    // We handle elements of type IntModN separately, since we can sample them
    // together.
    int num_ints_mod_n = 0;
    int num_other = 0;
    const ValueType* int_mod_n = nullptr;
    int bitsize_ints_mod_n = 0;
    int bitsize_other = 0;
    for (const ValueType& el : value_type.tuple().elements()) {
      if (el.type_case() == ValueType::kIntModN) {
        // Element is integer mod N -> check if it is the same as the others in
        // this tuple and increase counter.
        if (!int_mod_n) {
          int_mod_n = &el;
        } else {
          absl::StatusOr<bool> types_are_equal =
              ValueTypesAreEqual(el, *int_mod_n);
          if (!types_are_equal.ok()) {
            return types_are_equal.status();
          }
          if (!*types_are_equal) {
            return absl::UnimplementedError(
                "All elements of type IntModN in a tuple must be the same");
          }
        }
        ++num_ints_mod_n;
      } else {
        ++num_other;
      }
    }
    if (num_other > 0) {
      for (int i = 0; i < num_other; ++i) {
        double per_element_security_parameter =
            security_parameter + std::log2(static_cast<double>(num_other));
        DPF_ASSIGN_OR_RETURN(int el_bitsize,
                             BitsNeeded(value_type.tuple().elements(i),
                                        per_element_security_parameter));
        bitsize_other += el_bitsize;
      }
    }
    if (num_ints_mod_n > 0) {
      DPF_ASSIGN_OR_RETURN(
          absl::uint128 modulus,
          ValueIntegerToUint128(int_mod_n->int_mod_n().modulus()));
      DPF_ASSIGN_OR_RETURN(
          int64_t bytes_needed_ints_mod_n,
          dpf_internal::IntModNBase::GetNumBytesRequired(
              num_ints_mod_n, int_mod_n->int_mod_n().base_integer().bitsize(),
              modulus, security_parameter));
      bitsize_ints_mod_n = bytes_needed_ints_mod_n * 8;
    }
    return bitsize_ints_mod_n + bitsize_other;
  } else if (value_type.type_case() == ValueType::kIntModN) {
    DPF_ASSIGN_OR_RETURN(
        absl::uint128 modulus,
        ValueIntegerToUint128(value_type.int_mod_n().modulus()));
    DPF_ASSIGN_OR_RETURN(int64_t bytes_needed_ints_mod_n,
                         dpf_internal::IntModNBase::GetNumBytesRequired(
                             1, value_type.int_mod_n().base_integer().bitsize(),
                             modulus, security_parameter));
    return 8 * bytes_needed_ints_mod_n;
  } else if (value_type.type_case() == ValueType::kXorWrapper) {
    return value_type.xor_wrapper().bitsize();
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "BitsNeeded: Unsupported ValueType:\n", value_type.DebugString()));
}

// Integer Helpers

Value::Integer Uint128ToValueInteger(absl::uint128 input) {
  Value::Integer result;
  if (absl::Uint128High64(input) == 0) {
    result.set_value_uint64(absl::Uint128Low64(input));
  } else {
    Block& block = *(result.mutable_value_uint128());
    block.set_high(absl::Uint128High64(input));
    block.set_low(absl::Uint128Low64(input));
  }
  return result;
}

absl::StatusOr<absl::uint128> ValueIntegerToUint128(const Value::Integer& in) {
  if (in.value_case() == Value::Integer::kValueUint128) {
    return absl::MakeUint128(in.value_uint128().high(),
                             in.value_uint128().low());
  } else if (in.value_case() == Value::Integer::kValueUint64) {
    return in.value_uint64();
  }
  return absl::InvalidArgumentError(
      "Unknown value case for the given integer Value");
}

}  // namespace dpf_internal
}  // namespace distributed_point_functions

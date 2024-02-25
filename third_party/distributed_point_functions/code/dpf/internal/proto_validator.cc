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

#include "dpf/internal/proto_validator.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "dpf/distributed_point_function.pb.h"
#include "dpf/internal/value_type_helpers.h"
#include "dpf/status_macros.h"
#include "google/protobuf/repeated_field.h"

namespace distributed_point_functions {
namespace dpf_internal {

namespace {

inline double GetDefaultSecurityParameter(const DpfParameters& parameters) {
  return ProtoValidator::kDefaultSecurityParameter +
         parameters.log_domain_size();
}

inline bool AlmostEqual(double a, double b) {
  return std::abs(a - b) <= ProtoValidator::kSecurityParameterEpsilon;
}

absl::StatusOr<bool> ParametersAreEqual(const DpfParameters& lhs,
                                        const DpfParameters& rhs) {
  if (lhs.log_domain_size() != rhs.log_domain_size()) {
    return false;
  }
  if (!(
          // There are three ways that security parameters can be equivalent.
          // Both equal.
          AlmostEqual(lhs.security_parameter(), rhs.security_parameter()) ||
          // lhs is zero and rhs has the default value.
          (lhs.security_parameter() == 0 &&
           AlmostEqual(rhs.security_parameter(),
                       GetDefaultSecurityParameter(rhs))) ||
          // rhs is zero and lhs has the default value.
          (rhs.security_parameter() == 0 &&
           AlmostEqual(lhs.security_parameter(),
                       GetDefaultSecurityParameter(lhs))))) {
    return false;
  }
  return ValueTypesAreEqual(lhs.value_type(), rhs.value_type());
}

absl::Status ValidateIntegerType(const ValueType::Integer& type) {
  int bitsize = type.bitsize();
  if (bitsize < 1) {
    return absl::InvalidArgumentError("`bitsize` must be positive");
  }
  if (bitsize > 128) {
    return absl::InvalidArgumentError(
        "`bitsize` must be less than or equal to 128");
  }
  if ((bitsize & (bitsize - 1)) != 0) {
    return absl::InvalidArgumentError("`bitsize` must be a power of 2");
  }
  return absl::OkStatus();
}

absl::Status ValidateIntegerValue(const Value::Integer& value,
                                  const ValueType::Integer& type) {
  if (type.bitsize() < 128) {
    DPF_ASSIGN_OR_RETURN(absl::uint128 value_128, ValueIntegerToUint128(value));
    if (value_128 >= absl::uint128{1} << type.bitsize()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Value (= %d) too large for ValueType with bitsize = %d", value_128,
          type.bitsize()));
    }
  }
  return absl::OkStatus();
}

}  // namespace

ProtoValidator::ProtoValidator(std::vector<DpfParameters> parameters,
                               int tree_levels_needed,
                               absl::flat_hash_map<int, int> tree_to_hierarchy,
                               std::vector<int> hierarchy_to_tree)
    : parameters_(std::move(parameters)),
      tree_levels_needed_(tree_levels_needed),
      tree_to_hierarchy_(std::move(tree_to_hierarchy)),
      hierarchy_to_tree_(std::move(hierarchy_to_tree)) {}

absl::StatusOr<std::unique_ptr<ProtoValidator>> ProtoValidator::Create(
    absl::Span<const DpfParameters> parameters_in) {
  DPF_RETURN_IF_ERROR(ValidateParameters(parameters_in));

  // Set default values of security_parameter for all parameters.
  std::vector<DpfParameters> parameters(parameters_in.begin(),
                                        parameters_in.end());
  for (int i = 0; i < static_cast<int>(parameters.size()); ++i) {
    if (parameters[i].security_parameter() == 0) {
      parameters[i].set_security_parameter(
          GetDefaultSecurityParameter(parameters[i]));
    }
  }

  // Map hierarchy levels to levels in the evaluation tree for value correction,
  // and vice versa.
  absl::flat_hash_map<int, int> tree_to_hierarchy;
  std::vector<int> hierarchy_to_tree(parameters.size());
  // Also keep track of the height needed for the evaluation tree so far.
  int tree_levels_needed = 0;
  for (int i = 0; i < static_cast<int>(parameters.size()); ++i) {
    int log_bits_needed;
    DPF_ASSIGN_OR_RETURN(int bits_needed,
                         BitsNeeded(parameters[i].value_type(),
                                    parameters[i].security_parameter()));
    log_bits_needed = static_cast<int>(std::ceil(std::log2(bits_needed)));

    // The tree level depends on the domain size and the element size. A single
    // AES block can fit 128 = 2^7 bits, so usually tree_level ==
    // log_domain_size iff log_element_size >= 7. For smaller element sizes, we
    // can reduce the tree_level (and thus the height of the tree) by the
    // difference between log_element_size and 7. However, since the minimum
    // tree level is 0, we have to ensure that no two hierarchy levels map to
    // the same tree_level, hence the std::max.
    int tree_level =
        std::max(tree_levels_needed, parameters[i].log_domain_size() - 7 +
                                         std::min(log_bits_needed, 7));
    tree_to_hierarchy[tree_level] = i;
    hierarchy_to_tree[i] = tree_level;
    tree_levels_needed = std::max(tree_levels_needed, tree_level + 1);
  }

  return absl::WrapUnique(new ProtoValidator(
      std::move(parameters), tree_levels_needed, std::move(tree_to_hierarchy),
      std::move(hierarchy_to_tree)));
}

absl::Status ProtoValidator::ValidateParameters(
    absl::Span<const DpfParameters> parameters) {
  // Check that parameters are valid.
  if (parameters.empty()) {
    return absl::InvalidArgumentError("`parameters` must not be empty");
  }
  // Sentinel value for checking that domain sizes are increasing.
  int previous_log_domain_size = 0;
  for (int i = 0; i < static_cast<int>(parameters.size()); ++i) {
    // Check log_domain_size.
    int log_domain_size = parameters[i].log_domain_size();
    if (log_domain_size < 0) {
      return absl::InvalidArgumentError(
          "`log_domain_size` must be non-negative");
    }
    if (log_domain_size > 128) {
      return absl::InvalidArgumentError("`log_domain_size` must be <= 128");
    }
    if (i > 0 && log_domain_size <= previous_log_domain_size) {
      return absl::InvalidArgumentError(
          "`log_domain_size` fields must be in ascending order in "
          "`parameters`");
    }
    previous_log_domain_size = log_domain_size;

    if (parameters[i].has_value_type()) {
      DPF_RETURN_IF_ERROR(ValidateValueType(parameters[i].value_type()));
    } else {
      return absl::InvalidArgumentError("`value_type` is required");
    }

    if (std::isnan(parameters[i].security_parameter())) {
      return absl::InvalidArgumentError("`security_parameter` must not be NaN");
    }
    if (parameters[i].security_parameter() < 0 ||
        parameters[i].security_parameter() > 128) {
      // Since we use AES-128 for the PRG, a security parameter of > 128 is not
      // possible.
      return absl::InvalidArgumentError(
          "`security_parameter` must be in [0, 128]");
    }
  }
  return absl::OkStatus();
}

absl::Status ProtoValidator::ValidateDpfKey(const DpfKey& key) const {
  // Check that `key` has the seed and last_level_output_correction set.
  if (!key.has_seed()) {
    return absl::InvalidArgumentError("key.seed must be present");
  }
  if (key.last_level_value_correction().empty()) {
    return absl::InvalidArgumentError(
        "key.last_level_value_correction must be present");
  }
  // Check that `key` is valid for the DPF defined by `parameters_`.
  if (key.correction_words_size() != tree_levels_needed_ - 1) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Malformed DpfKey: expected ", tree_levels_needed_ - 1,
        " correction words, but got ", key.correction_words_size()));
  }
  for (int i = 0; i < static_cast<int>(hierarchy_to_tree_.size()); ++i) {
    if (hierarchy_to_tree_[i] == tree_levels_needed_ - 1) {
      // The output correction of the last tree level is always stored in
      // last_level_output_correction.
      continue;
    }
    ABSL_DCHECK(hierarchy_to_tree_[i] < key.correction_words_size());
    if (key.correction_words(hierarchy_to_tree_[i])
            .value_correction()
            .empty()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Malformed DpfKey: expected correction_words[", hierarchy_to_tree_[i],
          "] to contain the value correction of hierarchy level ", i));
    }
  }
  return absl::OkStatus();
}

absl::Status ProtoValidator::ValidateEvaluationContext(
    const EvaluationContext& ctx) const {
  if (ctx.parameters_size() != static_cast<int>(parameters_.size())) {
    return absl::InvalidArgumentError(
        "Number of parameters in `ctx` doesn't match");
  }
  for (int i = 0; i < ctx.parameters_size(); ++i) {
    DPF_ASSIGN_OR_RETURN(bool parameters_are_equal,
                         ParametersAreEqual(parameters_[i], ctx.parameters(i)));
    if (!parameters_are_equal) {
      return absl::InvalidArgumentError(
          absl::StrCat("Parameter ", i, " in `ctx` doesn't match"));
    }
  }
  if (!ctx.has_key()) {
    return absl::InvalidArgumentError("ctx.key must be present");
  }
  DPF_RETURN_IF_ERROR(ValidateDpfKey(ctx.key()));
  if (ctx.previous_hierarchy_level() >= ctx.parameters_size() - 1) {
    return absl::InvalidArgumentError(
        "This context has already been fully evaluated");
  }
  if (!ctx.partial_evaluations().empty() &&
      ctx.partial_evaluations_level() > ctx.previous_hierarchy_level()) {
    return absl::InvalidArgumentError(
        "ctx.partial_evaluations_level must be less than or equal to "
        "ctx.previous_hierarchy_level");
  }
  return absl::OkStatus();
}

absl::Status ProtoValidator::ValidateValueType(const ValueType& value_type) {
  if (value_type.type_case() == ValueType::kInteger) {
    return ValidateIntegerType(value_type.integer());
  } else if (value_type.type_case() == ValueType::kTuple) {
    for (const ValueType& el : value_type.tuple().elements()) {
      DPF_RETURN_IF_ERROR(ValidateValueType(el));
    }
    return absl::OkStatus();
  } else if (value_type.type_case() == ValueType::kIntModN) {
    const ValueType::Integer& base_integer =
        value_type.int_mod_n().base_integer();
    DPF_RETURN_IF_ERROR(ValidateIntegerType(base_integer));
    return ValidateIntegerValue(value_type.int_mod_n().modulus(), base_integer);
  } else if (value_type.type_case() == ValueType::kXorWrapper) {
    return ValidateIntegerType(value_type.xor_wrapper());
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "ValidateValueType: Unsupported ValueType:\n", value_type.DebugString()));
}

absl::Status ProtoValidator::ValidateValue(const Value& value,
                                           const ValueType& type) {
  if (type.type_case() == ValueType::kInteger) {
    // Integers.
    if (value.value_case() != Value::kInteger) {
      return absl::InvalidArgumentError("Expected integer value");
    }
    return ValidateIntegerValue(value.integer(), type.integer());
  } else if (type.type_case() == ValueType::kTuple) {
    // Tuples.
    if (value.value_case() != Value::kTuple) {
      return absl::InvalidArgumentError("Expected tuple value");
    }
    if (value.tuple().elements_size() != type.tuple().elements_size()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Expected tuple value of size ", type.tuple().elements_size(),
          " but got size ", value.tuple().elements_size()));
    }
    for (int i = 0; i < type.tuple().elements_size(); ++i) {
      DPF_RETURN_IF_ERROR(
          ValidateValue(value.tuple().elements(i), type.tuple().elements(i)));
    }
    return absl::OkStatus();
  } else if (type.type_case() == ValueType::kIntModN) {
    DPF_RETURN_IF_ERROR(ValidateIntegerValue(value.int_mod_n(),
                                             type.int_mod_n().base_integer()));
    DPF_ASSIGN_OR_RETURN(absl::uint128 value_128,
                         ValueIntegerToUint128(value.int_mod_n()));
    DPF_ASSIGN_OR_RETURN(absl::uint128 modulus_128,
                         ValueIntegerToUint128(type.int_mod_n().modulus()));
    if (value_128 >= modulus_128) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Value (= %d) is too large for modulus (= %d)",
                          value_128, modulus_128));
    }
    return absl::OkStatus();
  } else if (type.type_case() == ValueType::kXorWrapper) {
    if (value.value_case() != Value::kXorWrapper) {
      return absl::InvalidArgumentError("Expected XorWrapper value");
    }
    return ValidateIntegerValue(value.xor_wrapper(), type.xor_wrapper());
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "ValidateValue: Unsupported ValueType:\n", type.DebugString()));
}

}  // namespace dpf_internal
}  // namespace distributed_point_functions

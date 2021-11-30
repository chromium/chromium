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

#include "dcf/distributed_comparison_function.h"

#include "dpf/status_macros.h"

namespace distributed_point_functions {

namespace {

void SetToZero(Value& value) {
  if (value.value_case() == Value::kInteger) {
    value.mutable_integer()->set_value_uint64(0);
  } else if (value.value_case() == Value::kIntModN) {
    value.mutable_int_mod_n()->set_value_uint64(0);
  } else if (value.value_case() == Value::kTuple) {
    for (int i = 0; i < value.tuple().elements_size(); ++i) {
      SetToZero(*(value.mutable_tuple()->mutable_elements(i)));
    }
  }
}

}  // namespace

DistributedComparisonFunction::DistributedComparisonFunction(
    DcfParameters parameters, std::unique_ptr<DistributedPointFunction> dpf)
    : parameters_(std::move(parameters)), dpf_(std::move(dpf)) {}

absl::StatusOr<std::unique_ptr<DistributedComparisonFunction>>
DistributedComparisonFunction::Create(const DcfParameters& parameters) {
  // A DCF with a single-element domain doesn't make sense.
  if (parameters.parameters().log_domain_size() < 1) {
    return absl::InvalidArgumentError("A DCF must have log_domain_size >= 1");
  }

  // We don't support the legacy element_bitsize field in DCFs.
  if (!parameters.parameters().has_value_type()) {
    return absl::InvalidArgumentError(
        "parameters.value_type must be set for "
        "DistributedComparisonFunction::Create");
  }

  // Create parameter vector for the incremental DPF.
  std::vector<DpfParameters> dpf_parameters(
      parameters.parameters().log_domain_size());
  for (int i = 0; i < static_cast<int>(dpf_parameters.size()); ++i) {
    dpf_parameters[i].set_log_domain_size(i);
    *(dpf_parameters[i].mutable_value_type()) =
        parameters.parameters().value_type();
  }

  // Check that parameters are valid. We can use the DPF proto validator
  // directly.
  DPF_RETURN_IF_ERROR(
      dpf_internal::ProtoValidator::ValidateParameters(dpf_parameters));

  // Create incremental DPF.
  DPF_ASSIGN_OR_RETURN(
      std::unique_ptr<DistributedPointFunction> dpf,
      DistributedPointFunction::CreateIncremental(dpf_parameters));

  return absl::WrapUnique(
      new DistributedComparisonFunction(parameters, std::move(dpf)));
}

absl::StatusOr<std::pair<DcfKey, DcfKey>>
DistributedComparisonFunction::GenerateKeys(absl::uint128 alpha,
                                            const Value& beta) {
  const int log_domain_size = parameters_.parameters().log_domain_size();
  std::vector<Value> dpf_values(log_domain_size, beta);
  for (int i = 0; i < log_domain_size; ++i) {
    // beta_i = 0 if alpha_i == 0, and beta otherwise.
    bool current_bit =
        (alpha & (absl::uint128{1} << (log_domain_size - i - 1))) != 0;
    if (!current_bit) {
      SetToZero(dpf_values[i]);
    }
  }

  std::pair<DcfKey, DcfKey> result;
  DPF_ASSIGN_OR_RETURN(
      std::tie(*(result.first.mutable_key()), *(result.second.mutable_key())),
      dpf_->GenerateKeysIncremental(
          alpha >> 1,  // We can ignore the last bit of alpha, since it is
                       // encoded in dpf_values.back().
          dpf_values));
  return result;
}

}  // namespace distributed_point_functions

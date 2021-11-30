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

#ifndef DISTRIBUTED_POINT_FUNCTIONS_DCF_DISTRIBUTED_COMPARISON_FUNCTION_H_
#define DISTRIBUTED_POINT_FUNCTIONS_DCF_DISTRIBUTED_COMPARISON_FUNCTION_H_

#include <memory>

#include "absl/meta/type_traits.h"
#include "absl/status/statusor.h"
#include "dcf/distributed_comparison_function.pb.h"
#include "dpf/distributed_point_function.h"
#include "dpf/distributed_point_function.pb.h"

namespace distributed_point_functions {

class DistributedComparisonFunction {
 public:
  static absl::StatusOr<std::unique_ptr<DistributedComparisonFunction>> Create(
      const DcfParameters& parameters);

  // Creates keys for a DCF that evaluates to shares of `beta` on any input x <
  // `alpha`, and shares of 0 otherwise.
  //
  // Returns INVALID_ARGUMENT if `alpha` or `beta` do not match the
  // DcfParameters passed at construction.
  //
  // Overload for explicit Value proto.
  absl::StatusOr<std::pair<DcfKey, DcfKey>> GenerateKeys(absl::uint128 alpha,
                                                         const Value& beta);

  // Template for automatic conversion to Value proto. Disabled if the argument
  // is convertible to `absl::uint128` or `Value` to make overloading
  // unambiguous.
  template <typename T, typename = absl::enable_if_t<
                            !std::is_convertible<T, Value>::value &&
                            is_supported_type_v<T>>>
  absl::StatusOr<std::pair<DcfKey, DcfKey>> GenerateKeys(absl::uint128 alpha,
                                                         const T& beta) {
    absl::StatusOr<Value> value = dpf_->ToValue(beta);
    if (!value.ok()) {
      return value.status();
    }
    return GenerateKeys(alpha, *value);
  }

  // Evaluates a DcfKey at the given point `x`.
  //
  // Returns INVALID_ARGUMENT if `key` or `x` do not match the parameters passed
  // at construction.
  template <typename T>
  absl::StatusOr<T> Evaluate(const DcfKey& key, absl::uint128 x);

  // DistributedComparisonFunction is neither copyable nor movable.
  DistributedComparisonFunction(const DistributedComparisonFunction&) = delete;
  DistributedComparisonFunction& operator=(
      const DistributedComparisonFunction&) = delete;

 private:
  DistributedComparisonFunction(DcfParameters parameters,
                                std::unique_ptr<DistributedPointFunction> dpf);

  const DcfParameters parameters_;
  const std::unique_ptr<DistributedPointFunction> dpf_;
};

// Implementation details.

template <typename T>
absl::StatusOr<T> DistributedComparisonFunction::Evaluate(const DcfKey& key,
                                                          absl::uint128 x) {
  const int log_domain_size = parameters_.parameters().log_domain_size();
  T result{};

  absl::StatusOr<EvaluationContext> ctx =
      dpf_->CreateEvaluationContext(key.key());
  if (!ctx.ok()) {
    return ctx.status();
  }

  int previous_bit = 0;
  for (int i = 0; i < log_domain_size; ++i) {
    absl::StatusOr<std::vector<T>> expansion_i;
    if (i == 0) {
      expansion_i = dpf_->EvaluateNext<T>({}, *ctx);
    } else {
      absl::uint128 prefix = 0;
      if (log_domain_size < 128) {
        prefix = x >> (log_domain_size - i + 1);
      }
      expansion_i =
          dpf_->EvaluateNext<T>(absl::MakeConstSpan(&prefix, 1), *ctx);
    }
    if (!expansion_i.ok()) {
      return expansion_i.status();
    }

    int current_bit = static_cast<int>(
        (x & (absl::uint128{1} << (log_domain_size - i - 1))) != 0);
    // We only add the current value along the path if the current bit of x is
    // 0.
    if (current_bit == 0) {
      result += (*expansion_i)[previous_bit];
    }
    previous_bit = current_bit;
  }
  return result;
}

}  // namespace distributed_point_functions

#endif  // DISTRIBUTED_POINT_FUNCTIONS_DCF_DISTRIBUTED_COMPARISON_FUNCTION_H_

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

#ifndef DISTRIBUTED_POINT_FUNCTIONS_DCF_FSS_GATES_MULTIPLE_INTERVAL_CONTAINMENT_H_
#define DISTRIBUTED_POINT_FUNCTIONS_DCF_FSS_GATES_MULTIPLE_INTERVAL_CONTAINMENT_H_
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/status/statusor.h"
#include "dcf/distributed_comparison_function.h"
#include "dcf/fss_gates/multiple_interval_containment.pb.h"
#include "dpf/status_macros.h"

namespace distributed_point_functions {
namespace fss_gates {

// Implements the Multiple Interval Containment gate as specified in
// https://eprint.iacr.org/2020/1392 (Fig. 14). Such a gate is specified by
// input and output group Z_{2 ^ n} where n is the bit length and a sequence
// of `m` public intervals {p_i, q_i}_{i \in [m]}. The Key generation procedure
// produces two keys, k_0 and k_1, corresponding to Party 0 and Party 1
// respectively. Evaluating each key on any point `x` in the input group results
// in an additive secret share of m values {y_i}_{i \in [m]} where y_i = `1`, if
// `p_i <= x <= q_i`, and 0 otherwise.

class MultipleIntervalContainmentGate {
 public:
  // Factory method : creates and returns a MultipleIntervalContainmentGate
  // initialized with appropriate parameters.
  static absl::StatusOr<std::unique_ptr<MultipleIntervalContainmentGate>>
  Create(const MicParameters& mic_parameters);

  // MultipleIntervalContainmentGate is neither copyable nor movable.
  MultipleIntervalContainmentGate(const MultipleIntervalContainmentGate&) =
      delete;
  MultipleIntervalContainmentGate& operator=(
      const MultipleIntervalContainmentGate&) = delete;

  // This method generates Multiple Interval Containment Gate a pair of keys
  // using `r_in` and `r_out` as the input mask and output masks respectively.
  // The implementation of this method is identical to Gen procedure specified
  // in https://eprint.iacr.org/2020/1392 (Fig. 14). Note that although the
  // datatype of r_in and r_out is absl::uint128, but they will be interpreted
  // as an element in the input and output group Z_{2 ^ n} respectively. This
  // reinterpretion in the group is achieved simply by taking mod of r_in and
  // r_out with the size of group i.e. 2^{log_group_size}.

  // This method expects that the size of r_out vector be exactly same as the
  // number of public intervals in the MicParameters. This is because for
  // each interval in MIC, we will need an independent output mask to hide
  // the actual cleartext output of the MIC on that interval. This method
  // will return InvalidArgumentError.

  // Returns INVALID_ARGUMENT if the size of r_out vector does not match the
  // number of intervals specified during construction.
  absl::StatusOr<std::pair<MicKey, MicKey>> Gen(
      absl::uint128 r_in, std::vector<absl::uint128> r_out);

  // This method evaluates the Multiple Interval Containment Gate key k
  // on input domain point `x`. The output is returned as a 128 bit string
  // and needs to be interpreted as an element in the output group Z_{2 ^ n}.
  absl::StatusOr<std::vector<absl::uint128>> Eval(MicKey k, absl::uint128 x);

 private:
  // Parameters needed for specifying a Multiple Interval Containment Gate.
  const MicParameters mic_parameters_;

  // Private constructor, called by `Create`
  MultipleIntervalContainmentGate(
      MicParameters mic_parameters,
      std::unique_ptr<DistributedComparisonFunction> dcf);

  // Pointer to a Distributed Comparison Function which will be internally
  // invoked by Gen and Eval.
  std::unique_ptr<DistributedComparisonFunction> dcf_;
};

}  // namespace fss_gates
}  // namespace distributed_point_functions

#endif  // DISTRIBUTED_POINT_FUNCTIONS_DCF_FSS_GATES_MULTIPLE_INTERVAL_CONTAINMENT_H_

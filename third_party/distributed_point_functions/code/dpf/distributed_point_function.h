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

#ifndef DISTRIBUTED_POINT_FUNCTIONS_DPF_DISTRIBUTED_POINT_FUNCTION_H_
#define DISTRIBUTED_POINT_FUNCTIONS_DPF_DISTRIBUTED_POINT_FUNCTION_H_

#include <openssl/cipher.h>

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "dpf/distributed_point_function.pb.h"
#include "dpf/internal/pseudorandom_generator.h"

namespace distributed_point_functions {

// Implements key generation and evaluation of distributed point functions.
// A distributed point function (DPF) is parameterized by an index `alpha` and a
// value `beta`. The key generation procedure produces two keys `k_a`, `k_b`.
// Evaluating each key on any point `x` in the DPF domain results in an additive
// secret share of `beta`, if `x == alpha`, and a share of 0 otherwise. This
// class also supports *incremental* DPFs that can additionally be evaluated on
// prefixes of points, resulting in different values `beta_i`for each prefix of
// `alpha`.
class DistributedPointFunction {
 public:
  // Creates a new instance of a distributed point function that can be
  // evaluated only at the output layer.
  //
  // Returns INVALID_ARGUMENT if the parameters are invalid.
  static absl::StatusOr<std::unique_ptr<DistributedPointFunction>> Create(
      const DpfParameters& parameters);

  // Creates a new instance of an *incremental* DPF that can be evaluated at
  // multiple layers. Each parameter set in `parameters` should specify the
  // domain size and element size at one of the layers to be evaluated, in
  // increasing domain size order. Element sizes must be non-decreasing.
  //
  // Returns INVALID_ARGUMENT if the parameters are invalid.
  static absl::StatusOr<std::unique_ptr<DistributedPointFunction>>
  CreateIncremental(absl::Span<const DpfParameters> parameters);

  // DistributedPointFunction is neither copyable nor movable.
  DistributedPointFunction(const DistributedPointFunction&) = delete;
  DistributedPointFunction& operator=(const DistributedPointFunction&) = delete;

  // Generates a pair of keys for a DPF that evaluates to `beta` when evaluated
  // `alpha`. Returns INVALID_ARGUMENT if used on an incremental DPF with more
  // than one set of parameters, or if `alpha` or `beta` are outside of the
  // domains specified at construction.
  absl::StatusOr<std::pair<DpfKey, DpfKey>> GenerateKeys(
      absl::uint128 alpha, absl::uint128 beta) const;

  // Generates a pair of keys for an incremental DPF. For each parameter i
  // passed at construction, the DPF evaluates to `beta[i]` at the first
  // `parameters_[i].log_domain_size()` bits of `alpha`.
  // Returns INVALID_ARGUMENT if `beta.size() != parameters_.size()` or if
  // `alpha` or any element of `beta` are outside of the domains specified at
  // construction.
  absl::StatusOr<std::pair<DpfKey, DpfKey>> GenerateKeysIncremental(
      absl::uint128 alpha, absl::Span<const absl::uint128> beta) const;

  // Returns an `EvaluationContext` for incrementally evaluating the given
  // DpfKey.
  //
  // Returns INVALID_ARGUMENT if `key` doesn't match the parameters given at
  // construction.
  absl::StatusOr<EvaluationContext> CreateEvaluationContext(DpfKey key);

  // Evaluates the given `hierarchy_level` of the DPF under all `prefixes`
  // passed to this function. If `prefixes` is empty, evaluation starts from the
  // seed of `ctx.key`. Otherwise, each element of `prefixes` must fit in the
  // domain size of `ctx.previous_hierarchy_level`. Further, `prefixes` may only
  // contain extensions of the prefixes passed in the previous call. For
  // example, in the following sequence of calls, for each element p2 of
  // `prefixes2`, there must be an element p1 of `prefixes1` such that p1 is a
  // prefix of p2:
  //
  //   DPF_ASSIGN_OR_RETURN(std::unique_ptr<EvaluationContext> ctx,
  //                        dpf->CreateEvaluationContext(key));
  //   using T0 = ...;
  //   DPF_ASSIGN_OR_RETURN(std::vector<T0> evaluations0,
  //                        dpf->EvaluateUntil(0, {}, *ctx));
  //
  //   std::vector<absl::uint128> prefixes1 = ...;
  //   using T1 = ...;
  //   DPF_ASSIGN_OR_RETURN(std::vector<T1> evaluations1,
  //                        dpf->EvaluateUntil(1, prefixes1, *ctx));
  //   ...
  //   std::vector<absl::uint128> prefixes2 = ...;
  //   using T2 = ...;
  //   DPF_ASSIGN_OR_RETURN(std::vector<T2> evaluations2,
  //                        dpf->EvaluateUntil(3, prefixes2, *ctx));
  //
  // The prefixes are read from the lowest-order bits of the corresponding
  // absl::uint128. The number of bits used for each prefix depends on the
  // output domain size of the previously evaluated hierarchy level. For
  // example, if `ctx` was last evaluated on a hierarchy level with output
  // domain size 2**20, then the 20 lowest-order bits of each element in
  // `prefixes` are used.
  //
  // Returns `INVALID_ARGUMENT` if
  //   - any element of `prefixes` is larger than the next hierarchy level's
  //     log_domain_size,
  //   - `prefixes` contains elements that are not extensions of previous
  //     prefixes, or
  //   - the bit-size of T doesn't match the next hierarchy level's
  //     element_bitsize.
  template <typename T>
  absl::StatusOr<std::vector<T>> EvaluateUntil(
      int hierarchy_level, absl::Span<const absl::uint128> prefixes,
      EvaluationContext& ctx) const;

  template <typename T>
  absl::StatusOr<std::vector<T>> EvaluateNext(
      absl::Span<const absl::uint128> prefixes, EvaluationContext& ctx) const {
    if (prefixes.empty()) {
      return EvaluateUntil<T>(0, prefixes, ctx);
    } else {
      return EvaluateUntil<T>(ctx.previous_hierarchy_level() + 1, prefixes,
                              ctx);
    }
  }

 private:
  // Private constructor, called by `CreateIncremental`.
  DistributedPointFunction(std::vector<DpfParameters> parameters,
                           int tree_levels_needed,
                           absl::flat_hash_map<int, int> tree_to_hierarchy,
                           std::vector<int> hierarchy_to_tree,
                           dpf_internal::PseudorandomGenerator prg_left,
                           dpf_internal::PseudorandomGenerator prg_right,
                           dpf_internal::PseudorandomGenerator prg_value);

  // Computes the value correction for the given `hierarchy_level`, `seeds`,
  // index `alpha` and value `beta`. If `invert` is true, the individual values
  // in the returned block are multiplied element-wise by -1. Expands `seeds`
  // using `prg_ctx_value_`, then calls ComputeValueCorrectionFor<T> for the
  // right type depending on `parameters_[hierarchy_level].element_bitsize()`.
  //
  // Returns INTERNAL in case the PRG expansion fails, and UNIMPLEMENTED if
  // `element_bitsize` is not supported.
  absl::StatusOr<absl::uint128> ComputeValueCorrection(
      int hierarchy_level, absl::Span<const absl::uint128> seeds,
      absl::uint128 alpha, absl::uint128 beta, bool invert) const;

  // Expands the PRG seeds at the next `tree_level` for an incremental DPF with
  // index `alpha` and values `beta`, updates `seeds` and `control_bits`, and
  // writes the next correction word to `keys`. Called from
  // `GenerateKeysIncremental`.
  absl::Status GenerateNext(int tree_level, absl::uint128 alpha,
                            absl::Span<const absl::uint128> beta,
                            absl::Span<absl::uint128> seeds,
                            absl::Span<bool> control_bits,
                            absl::Span<DpfKey> keys) const;

  // Checks if the parameters of `ctx` are compatible with this DPF. Returns OK
  // if that's the case, and INVALID_ARGUMENT otherwise.
  absl::Status CheckContextParameters(const EvaluationContext& ctx) const;

  // Computes the tree index (representing a path in the FSS tree) from the
  // given `domain_index` and `hierarchy_level`. Does NOT check whether the
  // given domain index fits in the domain at `hierarchy_level`.
  absl::uint128 DomainToTreeIndex(absl::uint128 domain_index,
                                  int hierarchy_level) const;

  // Computes the block index (pointing to an element in a batched 128-bit
  // block) from the given `domain_index` and `hierarchy_level`. Does NOT check
  // whether the given domain index fits in the domain at `hierarchy_level`.
  int DomainToBlockIndex(absl::uint128 domain_index, int hierarchy_level) const;

  // BitVector is a vector of bools. Allows for faster access times than
  // std::vector<bool>, as well as inlining if the size is small.
  using BitVector = absl::InlinedVector<bool, 8 / sizeof(bool)>;

  // Seeds and control bits resulting from a DPF expansion. This type is
  // returned by `ExpandSeeds` and `ExpandAndUpdateContext`.
  struct DpfExpansion {
    std::vector<absl::uint128> seeds;
    BitVector control_bits;
  };

  // Performs DPF evaluation of the given `partial_evaluations` using
  // prg_ctx_left_ or prg_ctx_right_, and the given `correction_words`. At each
  // level `l < correction_words.size()`, the evaluation for the i-th seed in
  // `partial_evaluations` continues along the left or right path depending on
  // the l-th most significant bit among the lowest `correction_words.size()`
  // bits of `paths[i]`.
  //
  // Returns INTERNAL in case of OpenSSL errors.
  absl::StatusOr<DpfExpansion> EvaluateSeeds(
      DpfExpansion partial_evaluations, absl::Span<const absl::uint128> paths,
      absl::Span<const CorrectionWord* const> correction_words) const;

  // Performs DPF expansion of the given `partial_evaluations` using
  // prg_ctx_left_ and prg_ctx_right_, and the given `correction_words`. In more
  // detail, each of the partial evaluations is subjected to a full subtree
  // expansion of `correction_words.size()` levels, and the concatenated result
  // is provided in the response. The result contains
  // `(partial_evaluations.size() * (2^correction_words.size())` evaluations in
  // a single `DpfExpansion`.
  //
  // Returns INTERNAL in case of OpenSSL errors.
  absl::StatusOr<DpfExpansion> ExpandSeeds(
      const DpfExpansion& partial_evaluations,
      absl::Span<const CorrectionWord* const> correction_words) const;

  // Computes partial evaluations of the paths to `prefixes` to be used as the
  // starting point of the expansion of `ctx`. If `update_ctx == true`, saves
  // the partial evaluations of `ctx.previous_hierarchy_level` to `ctx` and sets
  // `ctx.partial_evaluations_level` to `ctx.previous_hierarchy_level`.
  // Called by `ExpandAndUpdateContext`.
  //
  // Returns INVALID_ARGUMENT if any element of `prefixes` is not found in
  // `ctx.partial_evaluations()`, or `ctx.partial_evaluations()` contains
  // duplicate seeds.
  absl::StatusOr<DpfExpansion> ComputePartialEvaluations(
      absl::Span<const absl::uint128> prefixes, bool update_ctx,
      EvaluationContext& ctx) const;

  // Extracts the seeds for the given `prefixes` from `ctx` and expands them as
  // far as needed for the next hierarchy level. Returns the result as a
  // `DpfExpansion`. Called by `EvaluateUntil`, where the expanded seeds are
  // corrected to obtain output values.
  // After expansion, `ctx.hierarchy_level()` is increased. If this isn't the
  // last expansion, the expanded seeds are also saved in `ctx` for the next
  // expansion.
  //
  // Returns INVALID_ARGUMENT if any element of `prefixes` is not found in
  // `ctx.partial_evaluations()`, or `ctx.partial_evaluations()` contains
  // duplicate seeds. Returns INTERNAL in case of OpenSSL errors.
  absl::StatusOr<DpfExpansion> ExpandAndUpdateContext(
      int hierarchy_level, absl::Span<const absl::uint128> prefixes,
      EvaluationContext& ctx) const;

  // DP parameters passed to the factory function. Contains the domain size and
  // element size for hierarchy level of the incremental DPF.
  const std::vector<DpfParameters> parameters_;

  // Number of levels in the evaluation tree. This is always less than or equal
  // to the largest log_domain_size in parameters_.
  const int tree_levels_needed_;

  // Maps levels of the FSS evaluation tree to hierarchy levels (i.e., elements
  // of parameters_).
  const absl::flat_hash_map<int, int> tree_to_hierarchy_;

  // The inverse of tree_to_hierarchy_.
  const std::vector<int> hierarchy_to_tree_;

  // Pseudorandom generators for seed expansion (left and right), and value
  // correction.
  const dpf_internal::PseudorandomGenerator prg_left_;
  const dpf_internal::PseudorandomGenerator prg_right_;
  const dpf_internal::PseudorandomGenerator prg_value_;
};

}  // namespace distributed_point_functions

#endif  // DISTRIBUTED_POINT_FUNCTIONS_DPF_DISTRIBUTED_POINT_FUNCTION_H_

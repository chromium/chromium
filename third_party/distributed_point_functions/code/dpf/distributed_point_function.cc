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

#include "dpf/distributed_point_function.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "dpf/internal/evaluate_prg_hwy.h"
#include "dpf/internal/get_hwy_mode.h"
#include "dpf/internal/proto_validator.h"
#include "dpf/internal/value_type_helpers.h"
#include "dpf/status_macros.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "hwy/aligned_allocator.h"
#include "openssl/rand.h"

namespace distributed_point_functions {

namespace {

// PRG keys used to expand seeds using AES. The first two are used to compute
// correction words of seeds, while the last is used to compute correction
// words of the incremental DPF values. Values were computed by taking the
// first half of the SHA256 sum of the constant name, e.g., `echo
// "DistributedPointFunction::kPrgKeyLeft" | sha256sum`
constexpr absl::uint128 kPrgKeyLeft =
    absl::MakeUint128(0x5be037ccf6a03de5ULL, 0x935f08d0a5b6a2fdULL);
constexpr absl::uint128 kPrgKeyRight =
    absl::MakeUint128(0xef94b6aedebb026cULL, 0xe2ea1fe0f66f4d0bULL);
constexpr absl::uint128 kPrgKeyValue =
    absl::MakeUint128(0x05a5d1588c5423e3ULL, 0x46a31101b21d1c98ULL);

}  // namespace

DistributedPointFunction::DistributedPointFunction(
    std::unique_ptr<dpf_internal::ProtoValidator> proto_validator,
    std::vector<int> blocks_needed, Aes128FixedKeyHash prg_left,
    Aes128FixedKeyHash prg_right, Aes128FixedKeyHash prg_value,
    absl::flat_hash_map<std::string, ValueCorrectionFunction>
        value_correction_functions)
    : proto_validator_(std::move(proto_validator)),
      parameters_(proto_validator_->parameters()),
      tree_levels_needed_(proto_validator_->tree_levels_needed()),
      tree_to_hierarchy_(proto_validator_->tree_to_hierarchy()),
      hierarchy_to_tree_(proto_validator_->hierarchy_to_tree()),
      blocks_needed_(std::move(blocks_needed)),
      prg_left_(std::move(prg_left)),
      prg_right_(std::move(prg_right)),
      prg_value_(std::move(prg_value)),
      value_correction_functions_(value_correction_functions) {}

absl::StatusOr<std::vector<Value>>
DistributedPointFunction::ComputeValueCorrection(
    int hierarchy_level, absl::Span<const absl::uint128> seeds,
    absl::uint128 alpha, const Value& beta, bool invert) const {
  // Compute value output component of the PRG on current seeds. To that end, we
  // Compute x_0+0, ..., x_0+k-1, and x_1+0, ..., x_1+k-1, where x_i is the seed
  // for helper i, and k is the number of blocks needed at the current hierarchy
  // level. We use a single contiguous vector for both helpers, which allows us
  // to use a single call to prg_value_.Evaluate.
  int blocks_needed = blocks_needed_[hierarchy_level];
  std::vector<absl::uint128> expanded_seeds(2 * blocks_needed);
  absl::Span<absl::uint128> expanded_seed_a(&expanded_seeds[0], blocks_needed);
  absl::Span<absl::uint128> expanded_seed_b(&expanded_seeds[blocks_needed],
                                            blocks_needed);
  ABSL_DCHECK(seeds.size() == 2);
  std::iota(expanded_seed_a.begin(), expanded_seed_a.end(), seeds[0]);
  std::iota(expanded_seed_b.begin(), expanded_seed_b.end(), seeds[1]);

  // Evaluate PRG in place (this is safe as `Evaluate` creates a copy of the
  // input).
  DPF_RETURN_IF_ERROR(
      prg_value_.Evaluate(expanded_seeds, absl::MakeSpan(expanded_seeds)));

  // Compute index in block for alpha at the current hierarchy level.
  int index_in_block = DomainToBlockIndex(alpha, hierarchy_level);

  // Choose implementation depending on element_bitsize.
  DPF_ASSIGN_OR_RETURN(
      ValueCorrectionFunction func,
      GetValueCorrectionFunction(parameters_[hierarchy_level]));
  return func(
      absl::string_view(reinterpret_cast<const char*>(expanded_seed_a.data()),
                        blocks_needed * sizeof(absl::uint128)),
      absl::string_view(reinterpret_cast<const char*>(expanded_seed_b.data()),
                        blocks_needed * sizeof(absl::uint128)),
      index_in_block, beta, invert);
}

// Expands the PRG seeds at the next `tree_level`, updates `seeds` and
// `control_bits`, and writes the next correction word to `keys`.
absl::Status DistributedPointFunction::GenerateNext(
    int tree_level, absl::uint128 alpha, absl::Span<const Value> beta,
    absl::Span<absl::uint128> seeds, absl::Span<bool> control_bits,
    absl::Span<DpfKey> keys) const {
  // As in `GenerateKeysIncremental`, we annotate code with the corresponding
  // lines from https://arxiv.org/pdf/2012.14884.pdf#figure.caption.12.
  //
  // Lines 13 & 14: Compute value correction word if there is a value on the
  // current level. This is done here already, since we use the "PRG evaluation
  // optimization" described in Appendix C.2 of the paper. Since we are using
  // fixed-key AES as PRG, which can have arbitrary stretch, this optimization
  // works even for large output groups.
  CorrectionWord* correction_word = keys[0].add_correction_words();
  if (tree_to_hierarchy_.contains(tree_level - 1)) {
    int hierarchy_level = tree_to_hierarchy_.at(tree_level - 1);
    absl::uint128 alpha_prefix = 0;
    int shift_amount = parameters_.back().log_domain_size() -
                       parameters_[hierarchy_level].log_domain_size();
    if (shift_amount < 128) {
      alpha_prefix = alpha >> shift_amount;
    }
    DPF_ASSIGN_OR_RETURN(
        std::vector<Value> value_correction,
        ComputeValueCorrection(hierarchy_level, seeds, alpha_prefix,
                               beta[hierarchy_level], control_bits[1]));
    for (const Value& value : value_correction) {
      *(correction_word->add_value_correction()) = value;
    }
  }

  // Line 5: Expand seeds from previous level.
  std::array<std::array<absl::uint128, 2>, 2> expanded_seeds;
  DPF_RETURN_IF_ERROR(
      prg_left_.Evaluate(seeds, absl::MakeSpan(expanded_seeds[0])));
  DPF_RETURN_IF_ERROR(
      prg_right_.Evaluate(seeds, absl::MakeSpan(expanded_seeds[1])));
  std::array<std::array<bool, 2>, 2> expanded_control_bits;
  expanded_control_bits[0][0] =
      dpf_internal::ExtractAndClearLowestBit(expanded_seeds[0][0]);
  expanded_control_bits[0][1] =
      dpf_internal::ExtractAndClearLowestBit(expanded_seeds[0][1]);
  expanded_control_bits[1][0] =
      dpf_internal::ExtractAndClearLowestBit(expanded_seeds[1][0]);
  expanded_control_bits[1][1] =
      dpf_internal::ExtractAndClearLowestBit(expanded_seeds[1][1]);

  // Lines 6-8: Assign keep/lose branch depending on current bit of `alpha`.
  bool current_bit = 0;
  if (parameters_.back().log_domain_size() - tree_level < 128) {
    current_bit =
        (alpha & (absl::uint128{1}
                  << (parameters_.back().log_domain_size() - tree_level))) != 0;
  }
  bool keep = current_bit, lose = !current_bit;

  // Line 9: Compute seed correction word.
  absl::uint128 seed_correction =
      expanded_seeds[lose][0] ^ expanded_seeds[lose][1];

  // Line 10: Compute control bit correction words.
  std::array<bool, 2> control_bit_correction;
  control_bit_correction[0] = expanded_control_bits[0][0] ^
                              expanded_control_bits[0][1] ^ current_bit ^ 1;
  control_bit_correction[1] =
      expanded_control_bits[1][0] ^ expanded_control_bits[1][1] ^ current_bit;

  // We swap lines 11 and 12, since we first need to use the previous level's
  // control bits before updating them.

  // Line 12: Update seeds. Note that there is a typo in the paper: The
  // multiplication / AND needs to be done with the control bit of iteration
  // l-1, not l. Note that unlike the original algorithm, we are using the
  // corrected seed directly for the next iteration. This is secure as we're
  // using AES with a different key (kPrgKeyValue) to compute the value
  // correction word below.
  seeds[0] = expanded_seeds[keep][0];
  seeds[1] = expanded_seeds[keep][1];
  if (control_bits[0]) {
    seeds[0] ^= seed_correction;
  }
  if (control_bits[1]) {
    seeds[1] ^= seed_correction;
  }

  // Line 11: Update control bits.  Again, same typo as in Line 12.
  control_bits[0] = expanded_control_bits[keep][0] ^
                    (control_bits[0] && control_bit_correction[keep]);
  control_bits[1] = expanded_control_bits[keep][1] ^
                    (control_bits[1] && control_bit_correction[keep]);

  // Line 15: Assemble correction word and add it to keys[0].
  correction_word->mutable_seed()->set_high(
      absl::Uint128High64(seed_correction));
  correction_word->mutable_seed()->set_low(absl::Uint128Low64(seed_correction));
  correction_word->set_control_left(control_bit_correction[0]);
  correction_word->set_control_right(control_bit_correction[1]);

  // Copy correction word to second key.
  *(keys[1].add_correction_words()) = *correction_word;

  return absl::OkStatus();
}

absl::uint128 DistributedPointFunction::DomainToTreeIndex(
    absl::uint128 domain_index, int hierarchy_level) const {
  int block_index_bits = parameters_[hierarchy_level].log_domain_size() -
                         hierarchy_to_tree_[hierarchy_level];
  ABSL_DCHECK_LT(block_index_bits, 128);
  return domain_index >> block_index_bits;
}

int DistributedPointFunction::DomainToBlockIndex(absl::uint128 domain_index,
                                                 int hierarchy_level) const {
  int block_index_bits = parameters_[hierarchy_level].log_domain_size() -
                         hierarchy_to_tree_[hierarchy_level];
  ABSL_DCHECK_LT(block_index_bits, 128);
  return static_cast<int>(domain_index &
                          ((absl::uint128{1} << block_index_bits) - 1));
}

absl::Status DistributedPointFunction::EvaluateSeeds(
    absl::Span<const absl::uint128> seeds, absl::Span<const bool> control_bits,
    absl::Span<const absl::uint128> paths,
    absl::Span<const CorrectionWord* const> correction_words,
    absl::Span<absl::uint128> seeds_out,
    absl::Span<bool> control_bits_out) const {
  if (seeds.size() != control_bits.size() || seeds.size() != paths.size() ||
      seeds.size() != seeds_out.size() ||
      seeds.size() != control_bits_out.size()) {
    return absl::InvalidArgumentError(
        "`seeds`, `control_bits`, `paths`, `seeds_out`, and `control_bits_out` "
        "must all have equal sizes");
  }
  auto num_seeds = static_cast<int64_t>(seeds.size());
  auto num_levels = static_cast<int>(correction_words.size());
  if (num_seeds == 0 || num_levels == 0) {
    return absl::OkStatus();  // Nothing to do.
  }

  // Parse correction words for each level.
  auto correction_seeds = hwy::AllocateAligned<absl::uint128>(num_levels);
  if (correction_seeds == nullptr) {
    return absl::ResourceExhaustedError("Memory allocation error");
  }
  BitVector correction_controls_left(num_levels),
      correction_controls_right(num_levels);
  for (int level = 0; level < num_levels; ++level) {
    const CorrectionWord& correction = *(correction_words[level]);
    correction_seeds[level] =
        absl::MakeUint128(correction.seed().high(), correction.seed().low());
    correction_controls_left[level] = correction.control_left();
    correction_controls_right[level] = correction.control_right();
  }

  ABSL_DCHECK(seeds.size() == num_seeds);
  ABSL_DCHECK(control_bits.size() == num_seeds);
  ABSL_DCHECK(correction_controls_left.size() == num_levels);
  ABSL_DCHECK(correction_controls_right.size() == num_levels);
  ABSL_DCHECK(seeds_out.size() == num_seeds);
  ABSL_DCHECK(control_bits_out.size() == num_seeds);
  DPF_RETURN_IF_ERROR(dpf_internal::EvaluateSeeds(
      num_seeds, num_levels, num_levels, seeds.data(), control_bits.data(),
      paths.data(), 0, correction_seeds.get(), correction_controls_left.data(),
      correction_controls_right.data(), prg_left_, prg_right_, seeds_out.data(),
      control_bits_out.data()));
  return absl::OkStatus();
}

absl::StatusOr<DistributedPointFunction::DpfExpansion>
DistributedPointFunction::ExpandSeeds(
    const DpfExpansion& partial_evaluations,
    absl::Span<const CorrectionWord* const> correction_words) const {
  int num_expansions = static_cast<int>(correction_words.size());

  // Check that the output size fits in a size_t. This should already be checked
  // by the caller, so using ABSL_DCHECK here is enough.
  ABSL_DCHECK_LT(num_expansions, 63);
  auto current_level_size =
      static_cast<int64_t>(partial_evaluations.control_bits.size());
  absl::uint128 output_size_128 = absl::uint128{current_level_size}
                                  << num_expansions;
  ABSL_DCHECK_LE(output_size_128, std::numeric_limits<size_t>::max() / 2);
  size_t output_size = static_cast<size_t>(output_size_128);

  // Allocate buffers with the correct size to avoid reallocations.
  int64_t max_batch_size = Aes128FixedKeyHash::kBatchSize;
  std::vector<absl::uint128> prg_buffer_left(max_batch_size),
      prg_buffer_right(max_batch_size);

  // Copy seeds and control bits. We will swap these after every expansion.
  DpfExpansion expansion;
  expansion.seeds = hwy::AllocateAligned<absl::uint128>(output_size);
  if (expansion.seeds == nullptr) {
    return absl::ResourceExhaustedError("Memory allocation error");
  }
  std::copy_n(partial_evaluations.seeds.get(), current_level_size,
              expansion.seeds.get());
  expansion.control_bits = partial_evaluations.control_bits;
  expansion.control_bits.reserve(output_size);
  DpfExpansion next_level_expansion;
  next_level_expansion.seeds = hwy::AllocateAligned<absl::uint128>(output_size);
  if (next_level_expansion.seeds == nullptr) {
    return absl::ResourceExhaustedError("Memory allocation error");
  }
  next_level_expansion.control_bits.reserve(output_size);

  // We use an iterative expansion here to pipeline AES as much as possible.
  for (int i = 0; i < num_expansions; ++i) {
    next_level_expansion.control_bits.resize(0);
    absl::uint128 correction_seed = absl::MakeUint128(
        correction_words[i]->seed().high(), correction_words[i]->seed().low());
    bool correction_control_left = correction_words[i]->control_left();
    bool correction_control_right = correction_words[i]->control_right();
    // Expand PRG.
    for (int64_t start_block = 0; start_block < current_level_size;
         start_block += max_batch_size) {
      int64_t batch_size =
          std::min<int64_t>(current_level_size - start_block, max_batch_size);
      DPF_RETURN_IF_ERROR(prg_left_.Evaluate(
          absl::MakeConstSpan(expansion.seeds.get() + start_block, batch_size),
          absl::MakeSpan(prg_buffer_left).subspan(0, batch_size)));
      DPF_RETURN_IF_ERROR(prg_right_.Evaluate(
          absl::MakeConstSpan(expansion.seeds.get() + start_block, batch_size),
          absl::MakeSpan(prg_buffer_right).subspan(0, batch_size)));

      // Merge results into next level of seeds and perform correction.
      for (int64_t j = 0; j < batch_size; ++j) {
        const int64_t index_expanded = 2 * (start_block + j);
        if (expansion.control_bits[start_block + j]) {
          prg_buffer_left[j] ^= correction_seed;
          prg_buffer_right[j] ^= correction_seed;
        }
        next_level_expansion.seeds[index_expanded] = prg_buffer_left[j];
        next_level_expansion.seeds[index_expanded + 1] = prg_buffer_right[j];
        next_level_expansion.control_bits.push_back(
            dpf_internal::ExtractAndClearLowestBit(
                next_level_expansion.seeds[index_expanded]));
        next_level_expansion.control_bits.push_back(
            dpf_internal::ExtractAndClearLowestBit(
                next_level_expansion.seeds[index_expanded + 1]));
        if (expansion.control_bits[start_block + j]) {
          next_level_expansion.control_bits[index_expanded] ^=
              correction_control_left;
          next_level_expansion.control_bits[index_expanded + 1] ^=
              correction_control_right;
        }
      }
    }
    std::swap(expansion, next_level_expansion);
    current_level_size *= 2;
  }
  return expansion;
}

absl::StatusOr<DistributedPointFunction::DpfExpansion>
DistributedPointFunction::ComputePartialEvaluations(
    absl::Span<const absl::uint128> prefixes, int hierarchy_level,
    bool update_ctx, EvaluationContext& ctx) const {
  int64_t num_prefixes = static_cast<int64_t>(prefixes.size());

  DpfExpansion partial_evaluations;
  int start_level = hierarchy_to_tree_[ctx.partial_evaluations_level()];
  int stop_level = hierarchy_to_tree_[hierarchy_level];
  if (ctx.partial_evaluations_size() > 0 && start_level <= stop_level) {
    // We have partial evaluations from a tree level before the current one.
    // Parse `ctx.partial_evaluations` into a btree_map for quick lookups up by
    // prefix. We use a btree_map because `ctx.partial_evaluations()` will
    // usually be sorted.
    absl::btree_map<absl::uint128, std::pair<absl::uint128, bool>>
        previous_partial_evaluations;
    for (const PartialEvaluation& element : ctx.partial_evaluations()) {
      absl::uint128 prefix =
          absl::MakeUint128(element.prefix().high(), element.prefix().low());
      // Try inserting `(seed, control_bit)` at `prefix` into
      // partial_evaluations. Return an error if `prefix` is already present
      // with a different seed or control bit.
      auto value = std::make_pair(
          absl::MakeUint128(element.seed().high(), element.seed().low()),
          element.control_bit());
      auto it = previous_partial_evaluations.try_emplace(
          previous_partial_evaluations.end(), prefix, value);
      if (it->second != value) {
        return absl::InvalidArgumentError(
            "Duplicate prefix in `ctx.partial_evaluations()` with mismatching "
            "seed or control bit");
      }
    }
    // Now select all partial evaluations from the map that correspond to
    // `prefixes`.
    partial_evaluations.seeds =
        hwy::AllocateAligned<absl::uint128>(num_prefixes);
    if (partial_evaluations.seeds == nullptr) {
      return absl::ResourceExhaustedError("Memory allocation error");
    }
    partial_evaluations.control_bits.reserve(num_prefixes);
    for (int64_t i = 0; i < num_prefixes; ++i) {
      absl::uint128 previous_prefix = 0;
      if (stop_level - start_level < 128) {
        previous_prefix = prefixes[i] >> (stop_level - start_level);
      }
      auto it = previous_partial_evaluations.find(previous_prefix);
      if (it == previous_partial_evaluations.end()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Prefix not present in ctx.partial_evaluations at hierarchy level ",
            hierarchy_level));
      }
      const std::pair<absl::uint128, bool>& partial_evaluation = it->second;
      partial_evaluations.seeds[partial_evaluations.control_bits.size()] =
          partial_evaluation.first;
      partial_evaluations.control_bits.push_back(partial_evaluation.second);
    }
  } else {
    // No partial evaluations in `ctx` -> Start from the beginning.
    partial_evaluations.seeds =
        hwy::AllocateAligned<absl::uint128>(num_prefixes);
    if (partial_evaluations.seeds == nullptr) {
      return absl::ResourceExhaustedError("Memory allocation error");
    }
    auto seeds = absl::MakeSpan(partial_evaluations.seeds.get(), num_prefixes);
    std::fill(
        seeds.begin(), seeds.end(),
        absl::MakeUint128(ctx.key().seed().high(), ctx.key().seed().low()));
    partial_evaluations.control_bits.resize(
        num_prefixes, static_cast<bool>(ctx.key().party()));
    start_level = 0;
  }

  // Evaluate the DPF up to current_tree_level.
  auto seeds = absl::MakeSpan(partial_evaluations.seeds.get(),
                              partial_evaluations.control_bits.size());
  DPF_RETURN_IF_ERROR(
      EvaluateSeeds(seeds, partial_evaluations.control_bits, prefixes,
                    absl::MakeConstSpan(ctx.key().correction_words())
                        .subspan(start_level, stop_level - start_level),
                    seeds, absl::MakeSpan(partial_evaluations.control_bits)));

  // Update `partial_evaluations` in `ctx` if there are more evaluations to
  // come.
  ctx.clear_partial_evaluations();
  ctx.mutable_partial_evaluations()->Reserve(num_prefixes);
  if (update_ctx) {
    for (int64_t i = 0; i < num_prefixes; ++i) {
      PartialEvaluation* current_element = ctx.add_partial_evaluations();
      current_element->mutable_prefix()->set_high(
          absl::Uint128High64(prefixes[i]));
      current_element->mutable_prefix()->set_low(
          absl::Uint128Low64(prefixes[i]));
      current_element->mutable_seed()->set_high(
          absl::Uint128High64(partial_evaluations.seeds[i]));
      current_element->mutable_seed()->set_low(
          absl::Uint128Low64(partial_evaluations.seeds[i]));
      current_element->set_control_bit(partial_evaluations.control_bits[i]);
    }
  }
  ctx.set_partial_evaluations_level(hierarchy_level);
  return partial_evaluations;
}

absl::StatusOr<DistributedPointFunction::DpfExpansion>
DistributedPointFunction::ExpandAndUpdateContext(
    int hierarchy_level, absl::Span<const absl::uint128> prefixes,
    EvaluationContext& ctx) const {
  // Expand seeds by expanding either the DPF key seed, or
  // `ctx.partial_evaluations` for the given `prefixes`.
  DpfExpansion selected_partial_evaluations;
  int start_level = 0;
  if (prefixes.empty()) {
    // First expansion -> Expand seed of the DPF key.
    selected_partial_evaluations.seeds = hwy::AllocateAligned<absl::uint128>(1);
    if (selected_partial_evaluations.seeds == nullptr) {
      return absl::ResourceExhaustedError("Memory allocation error");
    }
    selected_partial_evaluations.seeds[0] =
        absl::MakeUint128(ctx.key().seed().high(), ctx.key().seed().low());
    selected_partial_evaluations.control_bits = {
        static_cast<bool>(ctx.key().party())};
  } else {
    // Second or later expansion -> Extract all seeds for `prefixes` from
    // `ctx.partial_evaluations`. Update `ctx` if this is not the last
    // evaluation.
    bool update_ctx =
        (hierarchy_level < static_cast<int>(parameters_.size()) - 1);
    ABSL_DCHECK(ctx.previous_hierarchy_level() >= 0);
    DPF_ASSIGN_OR_RETURN(
        selected_partial_evaluations,
        ComputePartialEvaluations(prefixes, ctx.previous_hierarchy_level(),
                                  update_ctx, ctx));
    start_level = hierarchy_to_tree_[ctx.previous_hierarchy_level()];
  }

  // Expand up to the next hierarchy level.
  int stop_level = hierarchy_to_tree_[hierarchy_level];
  DPF_ASSIGN_OR_RETURN(
      DpfExpansion expansion,
      ExpandSeeds(selected_partial_evaluations,
                  absl::MakeConstSpan(ctx.key().correction_words())
                      .subspan(start_level, stop_level - start_level)));

  // Update hierarchy level in ctx.
  ctx.set_previous_hierarchy_level(hierarchy_level);
  return expansion;
}

absl::StatusOr<hwy::AlignedFreeUniquePtr<absl::uint128[]>>
DistributedPointFunction::HashExpandedSeeds(
    int hierarchy_level, absl::Span<const absl::uint128> expansion) const {
  const auto expansion_size = static_cast<int64_t>(expansion.size());
  const int blocks_needed = blocks_needed_[hierarchy_level];
  auto hashed_expansion =
      hwy::AllocateAligned<absl::uint128>(expansion_size * blocks_needed);
  if (hashed_expansion == nullptr) {
    return absl::ResourceExhaustedError("Memory allocation error");
  }
  for (int64_t i = 0; i < expansion_size; ++i) {
    for (int j = 0; j < blocks_needed; ++j) {
      hashed_expansion[i * blocks_needed + j] = expansion[i] + j;
    }
  }

  // Evaluate PRG in place (this is safe as `Evaluate` creates a copy of the
  // input).
  absl::Span<absl::uint128> hashed_expansion_span(
      hashed_expansion.get(), expansion_size * blocks_needed);
  DPF_RETURN_IF_ERROR(
      prg_value_.Evaluate(hashed_expansion_span, hashed_expansion_span));

  return hashed_expansion;
}

absl::StatusOr<std::string>
DistributedPointFunction::SerializeValueTypeDeterministically(
    const ValueType& value_type) {
  // We need to do serialization to a string by hand, in order to use
  // deterministic serialization.
  std::string serialized_value_type;
  {  // Start new block so that stream destructors are run before returning.
    ::google::protobuf::io::StringOutputStream string_stream(
        &serialized_value_type);
    ::google::protobuf::io::CodedOutputStream coded_stream(&string_stream);
    coded_stream.SetSerializationDeterministic(true);
    if (!value_type.SerializeToCodedStream(&coded_stream)) {
      return absl::InternalError("Serializing value_type to string failed");
    }
  }
  return serialized_value_type;
}

absl::StatusOr<DistributedPointFunction::ValueCorrectionFunction>
DistributedPointFunction::GetValueCorrectionFunction(
    const DpfParameters& parameters) const {
  std::string serialized_value_type;
  DPF_ASSIGN_OR_RETURN(
      serialized_value_type,
      SerializeValueTypeDeterministically(parameters.value_type()));
  auto it = value_correction_functions_.find(serialized_value_type);
  if (it == value_correction_functions_.end()) {
    return absl::FailedPreconditionError(absl::StrCat(
        "No value correction function known for the following parameters:\n",
        parameters.DebugString(),
        "Did you call RegisterValueType<T>() with your value type?"));
  }
  return it->second;
}

absl::StatusOr<std::unique_ptr<DistributedPointFunction>>
DistributedPointFunction::Create(const DpfParameters& parameters) {
  return CreateIncremental(absl::MakeConstSpan(&parameters, 1));
}

absl::StatusOr<std::unique_ptr<DistributedPointFunction>>
DistributedPointFunction::CreateIncremental(
    absl::Span<const DpfParameters> parameters) {
  // Log Highway mode for debugging.
  ABSL_LOG_FIRST_N(INFO, 1)
      << "Highway is in " << dpf_internal::GetHwyModeAsString() << " mode";

  // Validate `parameters` and store validator for later.
  DPF_ASSIGN_OR_RETURN(
      std::unique_ptr<dpf_internal::ProtoValidator> proto_validator,
      dpf_internal::ProtoValidator::Create(parameters));

  // Compute the number of value correction blocks needed for each hierarchy
  // level.
  std::vector<int> blocks_needed(parameters.size());
  for (int i = 0; i < static_cast<int>(parameters.size()); ++i) {
    DPF_ASSIGN_OR_RETURN(
        int bits_needed,
        dpf_internal::BitsNeeded(parameters[i].value_type(),
                                 parameters[i].security_parameter()));
    blocks_needed[i] = (bits_needed + 127) / 128;
  }

  // Set up hash functions for PRG.
  DPF_ASSIGN_OR_RETURN(Aes128FixedKeyHash prg_left,
                       Aes128FixedKeyHash::Create(kPrgKeyLeft));
  DPF_ASSIGN_OR_RETURN(Aes128FixedKeyHash prg_right,
                       Aes128FixedKeyHash::Create(kPrgKeyRight));
  DPF_ASSIGN_OR_RETURN(Aes128FixedKeyHash prg_value,
                       Aes128FixedKeyHash::Create(kPrgKeyValue));

  // For backwards compatibility, register all single unsigned integers as value
  // types.
  absl::flat_hash_map<std::string, ValueCorrectionFunction>
      value_correction_functions;
  DPF_RETURN_IF_ERROR(
      RegisterValueTypeImpl<uint8_t>(value_correction_functions));
  DPF_RETURN_IF_ERROR(
      RegisterValueTypeImpl<uint16_t>(value_correction_functions));
  DPF_RETURN_IF_ERROR(
      RegisterValueTypeImpl<uint32_t>(value_correction_functions));
  DPF_RETURN_IF_ERROR(
      RegisterValueTypeImpl<uint64_t>(value_correction_functions));
  DPF_RETURN_IF_ERROR(
      RegisterValueTypeImpl<absl::uint128>(value_correction_functions));

  // Copy parameters and return new DPF.
  return absl::WrapUnique(new DistributedPointFunction(
      std::move(proto_validator), std::move(blocks_needed), std::move(prg_left),
      std::move(prg_right), std::move(prg_value),
      std::move(value_correction_functions)));
}

absl::StatusOr<std::pair<DpfKey, DpfKey>>
DistributedPointFunction::GenerateKeysIncremental(
    absl::uint128 alpha, absl::Span<const Value> beta) {
  // Check validity of beta.
  if (beta.size() != parameters_.size()) {
    return absl::InvalidArgumentError(
        "`beta` has to have the same size as `parameters` passed at "
        "construction");
  }
  for (int i = 0; i < static_cast<int>(parameters_.size()); ++i) {
    absl::Status status = proto_validator_->ValidateValue(beta[i], i);
    if (!status.ok()) {
      return status;
    }
  }

  // Check validity of alpha.
  int last_level_log_domain_size = parameters_.back().log_domain_size();
  if (last_level_log_domain_size < 128 &&
      alpha >= (absl::uint128{1} << last_level_log_domain_size)) {
    return absl::InvalidArgumentError(
        "`alpha` must be smaller than the output domain size");
  }

  std::array<DpfKey, 2> keys;
  keys[0].set_party(0);
  keys[1].set_party(1);

  // We will annotate the following code with the corresponding lines from the
  // pseudocode in the Incremental DPF paper
  // (https://arxiv.org/pdf/2012.14884.pdf, Figure 11).
  //
  // There are two possible dimensions for each variable at each level: Parties
  // (0 or 1) and branches (left or right). For two-dimensional arrays, we use
  // the outer dimension for the branch, and the inner dimension for the party.
  //
  // Line 2: Sample random seeds for each party.
  std::array<absl::uint128, 2> seeds;
  RAND_bytes(reinterpret_cast<uint8_t*>(&seeds[0]), sizeof(absl::uint128));
  RAND_bytes(reinterpret_cast<uint8_t*>(&seeds[1]), sizeof(absl::uint128));
  keys[0].mutable_seed()->set_high(absl::Uint128High64(seeds[0]));
  keys[0].mutable_seed()->set_low(absl::Uint128Low64(seeds[0]));
  keys[1].mutable_seed()->set_high(absl::Uint128High64(seeds[1]));
  keys[1].mutable_seed()->set_low(absl::Uint128Low64(seeds[1]));

  // Line 3: Initialize control bits.
  std::array<bool, 2> control_bits{0, 1};

  // Line 4: Compute correction words for each level after the first one.
  keys[0].mutable_correction_words()->Reserve(tree_levels_needed_ - 1);
  keys[1].mutable_correction_words()->Reserve(tree_levels_needed_ - 1);
  for (int i = 1; i < tree_levels_needed_; i++) {
    DPF_RETURN_IF_ERROR(GenerateNext(i, alpha, beta, absl::MakeSpan(seeds),
                                     absl::MakeSpan(control_bits),
                                     absl::MakeSpan(keys)));
  }

  // Compute output correction word for last layer.
  DPF_ASSIGN_OR_RETURN(
      std::vector<Value> last_level_value_correction,
      ComputeValueCorrection(parameters_.size() - 1, seeds, alpha, beta.back(),
                             control_bits[1]));
  for (const Value& value : last_level_value_correction) {
    *(keys[0].add_last_level_value_correction()) = value;
    *(keys[1].add_last_level_value_correction()) = value;
  }

  return std::make_pair(std::move(keys[0]), std::move(keys[1]));
}

absl::StatusOr<EvaluationContext>
DistributedPointFunction::CreateEvaluationContext(DpfKey key) const {
  // Check that `key` is valid.
  DPF_RETURN_IF_ERROR(proto_validator_->ValidateDpfKey(key));

  // Create new EvaluationContext with `parameters_` and `key`.
  EvaluationContext result;
  for (int i = 0; i < static_cast<int>(parameters_.size()); ++i) {
    *(result.add_parameters()) = parameters_[i];
  }
  *(result.mutable_key()) = std::move(key);
  // previous_hierarchy_level = -1 means that this context has not been
  // evaluated at all.
  result.set_previous_hierarchy_level(-1);
  return result;
}

}  // namespace distributed_point_functions

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

#include "dpf/internal/evaluate_prg_hwy.h"

#include <cstdint>
#include <limits>

#include "absl/base/config.h"
#include "absl/base/optimization.h"
#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "dpf/status_macros.h"
#include "glog/logging.h"
#include "hwy/aligned_allocator.h"
#include "openssl/aes.h"

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "dpf/internal/evaluate_prg_hwy.cc"
#include "hwy/foreach_target.h"
// clang-format on

#include "dpf/internal/aes_128_fixed_key_hash_hwy.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace distributed_point_functions {
namespace dpf_internal {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

#if HWY_TARGET == HWY_SCALAR

absl::Status EvaluateSeedsHwy(
    int64_t num_seeds, int num_levels, const absl::uint128* seeds_in,
    const bool* control_bits_in, const absl::uint128* paths,
    const absl::uint128* correction_seeds, const bool* correction_controls_left,
    const bool* correction_controls_right, const Aes128FixedKeyHash& prg_left,
    const Aes128FixedKeyHash& prg_right, absl::uint128* seeds_out,
    bool* control_bits_out) {
  return EvaluateSeedsNoHwy(num_seeds, num_levels, seeds_in, control_bits_in,
                            paths, correction_seeds, correction_controls_left,
                            correction_controls_right, prg_left, prg_right,
                            seeds_out, control_bits_out);
}

#else

// Converts a bool array to a block-level mask suitable for vectors described by
// `d`. The mask value for each integer in the i-th block is set to input[i].
// If `max_blocks > 0`, returns after reading `max_blocks` bools from `input`.
template <typename D>
auto MaskFromBools(D d, const bool* input, int max_blocks = 0) {
  using T = hn::TFromD<D>;
  constexpr size_t ints_per_block = sizeof(absl::uint128) / sizeof(T);
  constexpr int buffer_size = std::max(HWY_MAX_BYTES / 8, 64);
  uint8_t mask_bits[buffer_size] = {0};
  for (int i = 0; i < hn::Lanes(d); ++i) {
    int block_idx = i / ints_per_block;
    if (max_blocks > 0 && block_idx >= max_blocks) {
      break;
    }
    if (input[block_idx]) {
      mask_bits[i / 8] |= uint8_t{1} << (i % 8);
    }
  }
  return hn::LoadMaskBits(d, mask_bits);
}

// Converts a mask for types `d` to a bool array. Assumes that the mask value
// for all integers in the i-th block is equal, and writes that value to
// output[i]. If `max_blocks > 0`, returns after writing `max_blocks` bools to
// `output`.
template <typename D, typename M>
void BoolsFromMask(D d, M mask, bool* output, int max_blocks = 0) {
  using T = hn::TFromD<D>;
  constexpr size_t ints_per_block = sizeof(absl::uint128) / sizeof(T);
  int num_outputs = hn::Lanes(d) / ints_per_block;
  if (max_blocks > 0) {
    num_outputs = max_blocks;
  }
  constexpr int buffer_size = std::max(HWY_MAX_BYTES / 8, 64);
  uint8_t mask_bits[buffer_size] = {0};
  hn::StoreMaskBits(d, mask, mask_bits);
  for (int i = 0; i < num_outputs; ++i) {
    int mask_idx = i * ints_per_block;
    output[i] = (mask_bits[mask_idx / 8] & (uint8_t{1} << (mask_idx % 8))) != 0;
  }
}

template <typename M>
M IfThenElseMask(M condition, M true_value, M false_value) {
  return hn::Or(hn::And(condition, true_value),
                hn::And(hn::Not(condition), false_value));
}

// Returns a mask that is `true` on all blocks where `input[i] & (1 << index)`
// is nonzero. The mask is a 64-bit-level mask, suitable for AES hashing.
template <typename V, typename D>
auto IsBitSet(D d, const V input, int index) {
  // First create a 128-bit block with the `index`-th bit set.
  HWY_ALIGN absl::uint128 shifted_index = absl::uint128{1} << index;

  // Now load it into a vector of 64-bit integers. Note that every second
  // element of that vector will be 0.
  const hn::Repartition<uint64_t, D> d64;
  static_assert(ABSL_IS_LITTLE_ENDIAN);
  const auto index_64 =
      hn::LoadDup128(d64, reinterpret_cast<const uint64_t*>(&shifted_index));

  // Compute input AND index_64 on 64-bit integers.
  auto input_64 = hn::BitCast(d64, input);
  input_64 = hn::And(input_64, index_64);

  // Take the OR of every two adjacent 64-bit integers. This ensures that each
  // half of an 128-bit block is nonzero iff at least one half was nonzero.
  input_64 = hn::Or(input_64, hn::Shuffle01(input_64));

  // Compute a 64-bit mask that checks which integers are nonzero.
  return hn::Ne(input_64, hn::Zero(d64));
}

// Dummy struct to get HWY_ALIGN as a number, for testing if an array of
// absl::uint128 is aligned.
struct HWY_ALIGN Aligned128 {
  absl::uint128 _;
};

absl::Status EvaluateSeedsHwy(
    int64_t num_seeds, int num_levels, const absl::uint128* seeds_in,
    const bool* control_bits_in, const absl::uint128* paths,
    const absl::uint128* correction_seeds, const bool* correction_controls_left,
    const bool* correction_controls_right, const Aes128FixedKeyHash& prg_left,
    const Aes128FixedKeyHash& prg_right, absl::uint128* seeds_out,
    bool* control_bits_out) {
  // Exit early if inputs are empty.
  if (num_seeds == 0 || num_levels == 0) {
    return absl::OkStatus();
  }

  // Check if inputs and outputs are aligned.
  constexpr size_t kHwyAlignment = alignof(Aligned128);
  const bool is_aligned =
      (reinterpret_cast<uintptr_t>(seeds_in) % kHwyAlignment == 0) &&
      (reinterpret_cast<uintptr_t>(paths) % kHwyAlignment == 0) &&
      (reinterpret_cast<uintptr_t>(correction_seeds) % kHwyAlignment == 0) &&
      (reinterpret_cast<uintptr_t>(seeds_out) % kHwyAlignment == 0);
  // Vector type used throughout this function: Largest byte vector available.
  const hn::ScalableTag<uint8_t> d8;
  // Only run the highway version if
  // - the inputs are aligned,
  // - the number of bytes in a vector is at least 16, and
  // - the number of bytes in a vector is a multiple of 16.
  if (ABSL_PREDICT_FALSE(!is_aligned || hn::Lanes(d8) < 16 ||
                         hn::Lanes(d8) % 16 != 0)) {
    return EvaluateSeedsNoHwy(num_seeds, num_levels, seeds_in, control_bits_in,
                              paths, correction_seeds, correction_controls_left,
                              correction_controls_right, prg_left, prg_right,
                              seeds_out, control_bits_out);
  }

  // Do AES key schedule.
  HWY_ALIGN AES_KEY expanded_key_0;
  HWY_ALIGN AES_KEY expanded_key_1;
  int openssl_status = AES_set_encrypt_key(
      reinterpret_cast<const uint8_t*>(&prg_left.key()), 128, &expanded_key_0);
  if (openssl_status != 0) {
    return absl::InternalError("Failed to set up AES key");
  }
  openssl_status = AES_set_encrypt_key(
      reinterpret_cast<const uint8_t*>(&prg_right.key()), 128, &expanded_key_1);
  if (openssl_status != 0) {
    return absl::InternalError("Failed to set up AES key");
  }

  // Helper variables.
  const hn::Repartition<uint64_t, decltype(d8)> d64;
  HWY_ALIGN absl::uint128 clear_lowest_bit_128 = ~absl::uint128{1};
  const auto clear_lowest_bit = hn::LoadDup128(
      d8, reinterpret_cast<const uint8_t*>(&clear_lowest_bit_128));
  const auto mask_all_zero = hn::FirstN(d64, 0);
  const auto mask_all_one = hn::Not(mask_all_zero);
  const int64_t num_bytes = num_seeds * sizeof(absl::uint128);

  // Pointer aliases for reading and writing data.
  const uint8_t* seeds_in_ptr = reinterpret_cast<const uint8_t*>(seeds_in);
  const uint8_t* paths_ptr = reinterpret_cast<const uint8_t*>(paths);
  uint8_t* seeds_out_ptr = reinterpret_cast<uint8_t*>(seeds_out);
  // Four vectors at a time.
  int64_t i = 0;
  for (; i + 4 * hn::Lanes(d8) <= num_bytes; i += 4 * hn::Lanes(d8)) {
    // Load initial seeds and paths into vectors.
    auto vec_0 = hn::Load(d8, seeds_in_ptr + i);
    auto vec_1 = hn::Load(d8, seeds_in_ptr + i + 1 * hn::Lanes(d8));
    auto vec_2 = hn::Load(d8, seeds_in_ptr + i + 2 * hn::Lanes(d8));
    auto vec_3 = hn::Load(d8, seeds_in_ptr + i + 3 * hn::Lanes(d8));
    const auto path_0 = hn::Load(d8, paths_ptr + i);
    const auto path_1 = hn::Load(d8, paths_ptr + i + 1 * hn::Lanes(d8));
    const auto path_2 = hn::Load(d8, paths_ptr + i + 2 * hn::Lanes(d8));
    const auto path_3 = hn::Load(d8, paths_ptr + i + 3 * hn::Lanes(d8));
    auto control_mask_0 =
        MaskFromBools(d64, control_bits_in + i / sizeof(absl::uint128));
    auto control_mask_1 = MaskFromBools(
        d64, control_bits_in + (i + 1 * hn::Lanes(d8)) / sizeof(absl::uint128));
    auto control_mask_2 = MaskFromBools(
        d64, control_bits_in + (i + 2 * hn::Lanes(d8)) / sizeof(absl::uint128));
    auto control_mask_3 = MaskFromBools(
        d64, control_bits_in + (i + 3 * hn::Lanes(d8)) / sizeof(absl::uint128));
    for (int j = 0; j < num_levels; ++j) {
      // Convert path bits to masks and evaluate PRG.
      const int bit_index = num_levels - j - 1;
      const auto path_mask_0 = IsBitSet(d8, path_0, bit_index);
      const auto path_mask_1 = IsBitSet(d8, path_1, bit_index);
      const auto path_mask_2 = IsBitSet(d8, path_2, bit_index);
      const auto path_mask_3 = IsBitSet(d8, path_3, bit_index);
      HashFourWithKeyMask(
          d8, vec_0, vec_1, vec_2, vec_3, path_mask_0, path_mask_1, path_mask_2,
          path_mask_3, reinterpret_cast<const uint8_t*>(expanded_key_0.rd_key),
          reinterpret_cast<const uint8_t*>(expanded_key_1.rd_key), vec_0, vec_1,
          vec_2, vec_3);

      // Apply correction.
      const auto correction_seed = hn::LoadDup128(
          d64, reinterpret_cast<const uint64_t*>(correction_seeds + j));
      vec_0 = hn::Xor(
          vec_0,
          hn::BitCast(d8, hn::IfThenElseZero(control_mask_0, correction_seed)));
      vec_1 = hn::Xor(
          vec_1,
          hn::BitCast(d8, hn::IfThenElseZero(control_mask_1, correction_seed)));
      vec_2 = hn::Xor(
          vec_2,
          hn::BitCast(d8, hn::IfThenElseZero(control_mask_2, correction_seed)));
      vec_3 = hn::Xor(
          vec_3,
          hn::BitCast(d8, hn::IfThenElseZero(control_mask_3, correction_seed)));

      // Extract control bit for next level.
      const auto next_control_mask_0 = IsBitSet(d8, vec_0, 0);
      const auto next_control_mask_1 = IsBitSet(d8, vec_1, 0);
      const auto next_control_mask_2 = IsBitSet(d8, vec_2, 0);
      const auto next_control_mask_3 = IsBitSet(d8, vec_3, 0);
      vec_0 = hn::And(vec_0, clear_lowest_bit);
      vec_1 = hn::And(vec_1, clear_lowest_bit);
      vec_2 = hn::And(vec_2, clear_lowest_bit);
      vec_3 = hn::And(vec_3, clear_lowest_bit);

      // Perform control bit correction.
      const auto correction_control_mask_left =
          correction_controls_left[j] ? mask_all_one : mask_all_zero;
      const auto correction_control_mask_right =
          correction_controls_right[j] ? mask_all_one : mask_all_zero;
      const auto correction_control_mask_0 =
          IfThenElseMask(path_mask_0, correction_control_mask_right,
                         correction_control_mask_left);
      const auto correction_control_mask_1 =
          IfThenElseMask(path_mask_1, correction_control_mask_right,
                         correction_control_mask_left);
      const auto correction_control_mask_2 =
          IfThenElseMask(path_mask_2, correction_control_mask_right,
                         correction_control_mask_left);
      const auto correction_control_mask_3 =
          IfThenElseMask(path_mask_3, correction_control_mask_right,
                         correction_control_mask_left);
      control_mask_0 =
          hn::Xor(next_control_mask_0,
                  (hn::And(control_mask_0, correction_control_mask_0)));
      control_mask_1 =
          hn::Xor(next_control_mask_1,
                  (hn::And(control_mask_1, correction_control_mask_1)));
      control_mask_2 =
          hn::Xor(next_control_mask_2,
                  (hn::And(control_mask_2, correction_control_mask_2)));
      control_mask_3 =
          hn::Xor(next_control_mask_3,
                  (hn::And(control_mask_3, correction_control_mask_3)));
    }
    // Write the evaluated outputs to memory.
    hn::Store(vec_0, d8, seeds_out_ptr + i);
    hn::Store(vec_1, d8, seeds_out_ptr + i + 1 * hn::Lanes(d8));
    hn::Store(vec_2, d8, seeds_out_ptr + i + 2 * hn::Lanes(d8));
    hn::Store(vec_3, d8, seeds_out_ptr + i + 3 * hn::Lanes(d8));
    BoolsFromMask(d64, control_mask_0,
                  control_bits_out + i / sizeof(absl::uint128));
    BoolsFromMask(
        d64, control_mask_1,
        control_bits_out + (i + 1 * hn::Lanes(d8)) / sizeof(absl::uint128));
    BoolsFromMask(
        d64, control_mask_2,
        control_bits_out + (i + 2 * hn::Lanes(d8)) / sizeof(absl::uint128));
    BoolsFromMask(
        d64, control_mask_3,
        control_bits_out + (i + 3 * hn::Lanes(d8)) / sizeof(absl::uint128));
  }
  DCHECK_GT(i + 4 * hn::Lanes(d8), num_bytes);

  // Single full vectors.
  for (; i + hn::Lanes(d8) <= num_bytes; i += hn::Lanes(d8)) {
    auto vec = hn::Load(d8, seeds_in_ptr + i);
    const auto path = hn::Load(d8, paths_ptr + i);
    auto control_mask =
        MaskFromBools(d64, control_bits_in + i / sizeof(absl::uint128));
    for (int j = 0; j < num_levels; ++j) {
      const int bit_index = num_levels - j - 1;
      const auto path_mask = IsBitSet(d8, path, bit_index);
      HashOneWithKeyMask(
          d8, vec, path_mask,
          reinterpret_cast<const uint8_t*>(expanded_key_0.rd_key),
          reinterpret_cast<const uint8_t*>(expanded_key_1.rd_key), vec);

      // Apply correction.
      const auto correction_seed = hn::LoadDup128(
          d64, reinterpret_cast<const uint64_t*>(correction_seeds + j));
      vec = hn::Xor(vec, hn::BitCast(d8, hn::IfThenElseZero(control_mask,
                                                            correction_seed)));
      // Extract control bit for next level.
      const auto next_control_mask = IsBitSet(d8, vec, 0);
      vec = hn::And(vec, clear_lowest_bit);

      // Perform control bit correction.
      const auto correction_control_mask_left =
          correction_controls_left[j] ? mask_all_one : mask_all_zero;
      const auto correction_control_mask_right =
          correction_controls_right[j] ? mask_all_one : mask_all_zero;

      const auto correction_control_mask =
          IfThenElseMask(path_mask, correction_control_mask_right,
                         correction_control_mask_left);
      control_mask = hn::Xor(next_control_mask,
                             (hn::And(control_mask, correction_control_mask)));
    }
    hn::Store(vec, d8, seeds_out_ptr + i);
    BoolsFromMask(d64, control_mask,
                  control_bits_out + i / sizeof(absl::uint128));
  }
  DCHECK_GT(i + hn::Lanes(d8), num_bytes);

  // Elements less than a full vector.
  int remaining_blocks = num_seeds - i / sizeof(absl::uint128);
  if (remaining_blocks > 0) {
    const int remaining_bytes = num_bytes - i;
    const int blocks_per_lane = hn::Lanes(d8) / sizeof(absl::uint128);
    // Copy to a buffer first, to ensure we have at least hn::Lanes(d8) bytes
    // to read. Calling MaskedLoad directly instead might lead to out-of-bounds
    // accesses.
    auto buffer = hwy::AllocateAligned<absl::uint128>(2 * blocks_per_lane);
    if (buffer == nullptr) {
      return absl::ResourceExhaustedError("Memory allocation error");
    }
    auto buffer_ptr = reinterpret_cast<uint8_t*>(buffer.get());
    std::copy_n(seeds_in + i / sizeof(absl::uint128), remaining_blocks,
                buffer.get());
    std::copy_n(paths + i / sizeof(absl::uint128), remaining_blocks,
                buffer.get() + blocks_per_lane);
    const auto load_mask = hn::FirstN(d8, remaining_bytes);
    auto vec = hn::MaskedLoad(load_mask, d8, buffer_ptr);
    const auto path = hn::MaskedLoad(load_mask, d8, buffer_ptr + hn::Lanes(d8));
    auto control_mask = MaskFromBools(
        d64, control_bits_in + i / sizeof(absl::uint128), remaining_blocks);
    for (int j = 0; j < num_levels; ++j) {
      const int bit_index = num_levels - j - 1;
      const auto path_mask = IsBitSet(d8, path, bit_index);
      HashOneWithKeyMask(
          d8, vec, path_mask,
          reinterpret_cast<const uint8_t*>(expanded_key_0.rd_key),
          reinterpret_cast<const uint8_t*>(expanded_key_1.rd_key), vec);
      // Perform seed correction.
      const auto correction_seed = hn::LoadDup128(
          d64, reinterpret_cast<const uint64_t*>(correction_seeds + j));
      vec = hn::Xor(vec, hn::BitCast(d8, hn::IfThenElseZero(control_mask,
                                                            correction_seed)));
      const auto next_control_mask = IsBitSet(d8, vec, 0);
      vec = hn::And(vec, clear_lowest_bit);
      const auto correction_control_mask_left =
          correction_controls_left[j] ? mask_all_one : mask_all_zero;
      const auto correction_control_mask_right =
          correction_controls_right[j] ? mask_all_one : mask_all_zero;
      const auto correction_control_mask =
          IfThenElseMask(path_mask, correction_control_mask_right,
                         correction_control_mask_left);
      control_mask = hn::Xor(next_control_mask,
                             (hn::And(control_mask, correction_control_mask)));
    }
    // Store back into buffer, then copy to seeds_out.
    hn::Store(vec, d8, buffer_ptr);
    std::copy_n(buffer.get(), remaining_blocks,
                seeds_out + i / sizeof(absl::uint128));
    BoolsFromMask(d64, control_mask,
                  control_bits_out + i / sizeof(absl::uint128),
                  remaining_blocks);
  }

  return absl::OkStatus();
}

#endif  // HWY_TARGET == HWY_SCALAR

}  // namespace HWY_NAMESPACE
}  // namespace dpf_internal
}  // namespace distributed_point_functions
HWY_AFTER_NAMESPACE();

#if HWY_ONCE || HWY_IDE
namespace distributed_point_functions {
namespace dpf_internal {

absl::Status EvaluateSeedsNoHwy(
    int64_t num_seeds, int num_levels, const absl::uint128* seeds_in,
    const bool* control_bits_in, const absl::uint128* paths,
    const absl::uint128* correction_seeds, const bool* correction_controls_left,
    const bool* correction_controls_right, const Aes128FixedKeyHash& prg_left,
    const Aes128FixedKeyHash& prg_right, absl::uint128* seeds_out,
    bool* control_bits_out) {
  using BitVector =
      absl::InlinedVector<bool,
                          std::max<size_t>(1, sizeof(bool*) / sizeof(bool))>;
  constexpr int64_t max_batch_size = Aes128FixedKeyHash::kBatchSize;

  // Allocate buffers.
  std::vector<absl::uint128> buffer_left, buffer_right;
  buffer_left.resize(max_batch_size);
  buffer_right.resize(max_batch_size);
  BitVector path_bits(max_batch_size), control_bits(max_batch_size);

  // Perform DPF evaluation in blocks.
  for (int64_t start_block = 0; start_block < num_seeds;
       start_block += max_batch_size) {
    int64_t current_batch_size =
        std::min<int64_t>(num_seeds - start_block, max_batch_size);

    for (int level = 0; level < num_levels; ++level) {
      // Evaluate PRG. We evaluate both left and right expansions, but only use
      // one of them (depending on path_bits). This seems to be faster than
      // first sorting the seeds by path_bits and then expanding.
      absl::Span<const absl::uint128> seeds =
          absl::MakeConstSpan((level == 0 ? seeds_in : seeds_out) + start_block,
                              current_batch_size);
      DPF_RETURN_IF_ERROR(prg_left.Evaluate(
          seeds, absl::MakeSpan(buffer_left).subspan(0, current_batch_size)));
      DPF_RETURN_IF_ERROR(prg_right.Evaluate(
          seeds, absl::MakeSpan(buffer_right).subspan(0, current_batch_size)));

      // Merge back into result.
      const int bit_index = num_levels - level - 1;
      for (int i = 0; i < current_batch_size; ++i) {
        path_bits[i] = 0;
        if (bit_index < 128) {
          path_bits[i] =
              (paths[start_block + i] & (absl::uint128{1} << bit_index)) != 0;
        }
        if (path_bits[i] == 0) {
          seeds_out[start_block + i] = buffer_left[i];
        } else {
          seeds_out[start_block + i] = buffer_right[i];
        }
      }

      // Compute correction. Making a copy here a copy here improves pipelining
      // by not updating result.control_bits in place. Do benchmarks before
      // removing this.
      std::copy_n(
          &(level == 0 ? control_bits_in : control_bits_out)[start_block],
          current_batch_size, &control_bits[0]);
      for (int i = 0; i < current_batch_size; ++i) {
        if (control_bits[i]) {
          seeds_out[start_block + i] ^= correction_seeds[level];
        }
        bool current_control_bit =
            ExtractAndClearLowestBit(seeds_out[start_block + i]);
        if (control_bits[i]) {
          if (path_bits[i] == 0) {
            current_control_bit ^= correction_controls_left[level];
          } else {
            current_control_bit ^= correction_controls_right[level];
          }
        }
        control_bits_out[start_block + i] = current_control_bit;
      }
    }
  }

  return absl::OkStatus();
}

HWY_EXPORT(EvaluateSeedsHwy);

absl::Status EvaluateSeeds(
    int64_t num_seeds, int num_levels, const absl::uint128* seeds_in,
    const bool* control_bits_in, const absl::uint128* paths,
    const absl::uint128* correction_seeds, const bool* correction_controls_left,
    const bool* correction_controls_right, const Aes128FixedKeyHash& prg_left,
    const Aes128FixedKeyHash& prg_right, absl::uint128* seeds_out,
    bool* control_bits_out) {
  return HWY_DYNAMIC_DISPATCH(EvaluateSeedsHwy)(
      num_seeds, num_levels, seeds_in, control_bits_in, paths, correction_seeds,
      correction_controls_left, correction_controls_right, prg_left, prg_right,
      seeds_out, control_bits_out);
}

}  // namespace dpf_internal
}  // namespace distributed_point_functions
#endif

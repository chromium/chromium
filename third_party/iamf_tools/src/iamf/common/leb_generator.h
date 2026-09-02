/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef COMMON_LEB_GENERATOR_H_
#define COMMON_LEB_GENERATOR_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

class LebGenerator {
 public:
  enum class GenerationMode {
    kMinimum,
    kFixedSize,
  };

  /*!\brief Factory function to create a `LebGenerator`.
   *
   * \param generation_mode Generation mode.
   * \param fixed_size Fixed size. When using `kGenerateLebFixedSize` mode it
   *        MUST be in the range [1, 8]. When using other modes it is ignored.
   * \return Unique pointer to `LebGenerator` on success. `nullptr` if the mode
   *         is unknown or `fixed_size` is invalid.
   */
  static std::unique_ptr<LebGenerator> Create(
      GenerationMode generation_mode = GenerationMode::kMinimum,
      int8_t fixed_size = 0);

  friend bool operator==(const LebGenerator& lhs,
                         const LebGenerator& rhs) = default;

  /*!\brief Encodes a `DecodedUleb128` to a vector representing a ULEB128.
   *
   * The behavior of the generator is controlled by `generation_mode_`. When
   * configured using `GenerationMode::kMinimum` values are generated using the
   * representation with the minimum number of bytes. When configured using
   * `GenerationMode::kFixedSize` values are generated using `fixed_size_` bytes
   * and generation may fail if this is not sufficient to encode the value.
   *
   * \param input Input value.
   * \param buffer Buffer to serialize to.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the generation fails.
   */
  absl::Status Uleb128ToUint8Vector(DecodedUleb128 input,
                                    std::vector<uint8_t>& buffer) const;

  /*!\brief Encodes a `DecodedSleb128` to a vector representing a SLEB128.
   *
   * The behavior of the generator is controlled by `generation_mode_`. When
   * configured using `GenerationMode::kMinimum` values are generated using the
   * representation with the minimum number of bytes. When configured using
   * `GenerationMode::kFixedSize` values are generated using `fixed_size_` bytes
   * and generation may fail if this is not sufficient to encode the value.
   *
   * \param input Input value.
   * \param buffer Buffer to serialize to.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the generation fails.
   */
  absl::Status Sleb128ToUint8Vector(DecodedSleb128 input,
                                    std::vector<uint8_t>& buffer) const;

 private:
  /*!\brief Constructor.
   *
   * \param generation_mode Generation mode.
   * \param fixed_size Fixed size. When using `kGenerateLebFixedSize` mode it
   *        MUST be in the range [1, 8]. When using other modes it is ignored.
   */
  LebGenerator(GenerationMode generation_mode, int8_t fixed_size);

  const GenerationMode generation_mode_;
  const int8_t fixed_size_;
};

}  // namespace iamf_tools

#endif  // COMMON_LEB_GENERATOR_H_

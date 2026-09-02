/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_ELEMENT_GAIN_OFFSET_CONFIG_H_
#define OBU_ELEMENT_GAIN_OFFSET_CONFIG_H_

#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief Implements Element Gain Offset Config from the IAMF spec. */
class ElementGainOffsetConfig {
 public:
  friend bool operator==(const ElementGainOffsetConfig& lhs,
                         const ElementGainOffsetConfig& rhs) = default;

  /*!\brief Creates a value-type `ElementGainOffsetConfig`.
   *
   * \param element_gain_offset Element gain offset.
   * \return Value-type `ElementGainOffsetConfig`.
   */
  static ElementGainOffsetConfig MakeValueType(
      QFormatOrFloatingPoint element_gain_offset);

  /*!\brief Creates a range-type `ElementGainOffsetConfig`.
   *
   * \param default_element_gain_offset Default element gain offset.
   * \param min_element_gain_offset Minimum element gain offset.
   * \param max_element_gain_offset Maximum element gain offset.
   * \return `ElementGainOffsetConfig` on success; a specific status on failure.
   */
  static absl::StatusOr<ElementGainOffsetConfig> CreateRangeType(
      QFormatOrFloatingPoint default_element_gain_offset,
      QFormatOrFloatingPoint min_element_gain_offset,
      QFormatOrFloatingPoint max_element_gain_offset);

  /*!\brief Creates an extension-type `ElementGainOffsetConfig`.
   *
   * \param element_gain_offset_config_type Type of the extension.
   * \param element_gain_offset_bytes Bytes of the extension.
   * \return `ElementGainOffsetConfig` on success; a specific status on failure.
   */
  static absl::StatusOr<ElementGainOffsetConfig> CreateExtensionType(
      uint8_t element_gain_offset_config_type,
      absl::Span<const uint8_t> element_gain_offset_bytes);

  /*!\brief Creates an `ElementGainOffsetConfig` from a `ReadBitBuffer`.
   *
   * \param rb `ReadBitBuffer` where the `ElementGainOffsetConfig` data is
   *        stored. Data read from the buffer is consumed.
   * \return `ElementGainOffsetConfig` on success; a specific status on failure.
   */
  static absl::StatusOr<ElementGainOffsetConfig> CreateFromBuffer(
      ReadBitBuffer& rb);

  /*!\brief Writes the `ElementGainOffsetConfig` to a `WriteBitBuffer`.
   *
   * \param wb `WriteBitBuffer` to write to.
   * \return `absl::OkStatus()` on success; a specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const;

  /*!\brief Prints logging information about the config.*/
  void Print() const;

 private:
  struct ValueType {
    QFormatOrFloatingPoint element_gain_offset;
    bool operator==(const ValueType&) const = default;

    absl::Status Write(WriteBitBuffer& wb) const;

    void Print() const;
  };
  struct RangeType {
    QFormatOrFloatingPoint default_element_gain_offset;
    QFormatOrFloatingPoint min_element_gain_offset;
    QFormatOrFloatingPoint max_element_gain_offset;
    bool operator==(const RangeType&) const = default;

    absl::Status Write(WriteBitBuffer& wb) const;

    void Print() const;
  };
  struct ExtensionType {
    uint8_t element_gain_offset_config_type;
    std::vector<uint8_t> element_gain_offset_bytes;
    bool operator==(const ExtensionType&) const = default;

    absl::Status Write(WriteBitBuffer& wb) const;

    void Print() const;
  };

  using ElementGainOffsetConfigVariant =
      std::variant<ValueType, RangeType, ExtensionType>;

  /*!\brief Private constructor.
   *
   * For use by static factory functions only.
   *
   * \param element_gain_offset_config_data `ElementGainOffsetConfigVariant` to
   *        construct from.
   */
  explicit ElementGainOffsetConfig(
      ElementGainOffsetConfigVariant&& element_gain_offset_config_data)
      : element_gain_offset_config_data_(
            std::move(element_gain_offset_config_data)) {}

  ElementGainOffsetConfigVariant element_gain_offset_config_data_;
};

}  // namespace iamf_tools

#endif  // OBU_ELEMENT_GAIN_OFFSET_CONFIG_H_

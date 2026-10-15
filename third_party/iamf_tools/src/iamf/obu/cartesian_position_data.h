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

#ifndef OBU_CARTESIAN_POSITION_DATA_H_
#define OBU_CARTESIAN_POSITION_DATA_H_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

/*!\brief Represents single 3D Cartesian position with animated coordinates.
 *
 * Coordinates are stored as right-justified signed 16-bit integers (`int16_t`):
 * - For 16-bit Cartesian positions, values span `[-32768, 32767]`.
 * - For 8-bit Cartesian positions, values are sign-extended into `int16_t` and
 *   span `[-128, 127]`.
 */
struct CartesianPosition {
  AnimatedParameterData<int16_t> x =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int16_t> y =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int16_t> z =
      AnimatedParameterData<int16_t>::MakeStep(0);

  bool friend operator==(const CartesianPosition& lhs,
                         const CartesianPosition& rhs) = default;
};

/*!\brief Bit depth of Cartesian coordinates. */
enum class CartesianBitDepth {
  k8Bit = 0,
  k16Bit = 1,
};

/*!\brief Parameter data for Cartesian positions (supports single/dual points,
 * 8-bit/16-bit).
 */
class CartesianPositionData : public ParameterData {
 public:
  CartesianPositionData() = default;

  /*!\brief Overridden destructor. */
  ~CartesianPositionData() override = default;

  /*!\brief Creates CartesianPositionData from a buffer.
   *
   * \param rb Buffer to read from.
   * \param bit_depth Bit depth of coordinates (8-bit or 16-bit).
   * \param is_dual Whether to read dual Cartesian positions (two points).
   * \return Deserialized CartesianPositionData or error.
   */
  static absl::StatusOr<CartesianPositionData> CreateFromBuffer(
      ReadBitBuffer& rb, CartesianBitDepth bit_depth, bool is_dual);

  /*!\brief Creates CartesianPositionData with a single Cartesian position.
   *
   * \param animation_type Animation type.
   * \param bit_depth Bit depth of coordinates (8-bit or 16-bit).
   * \param first_position First Cartesian position.
   * \return CartesianPositionData or error if coordinates out of bounds.
   */
  static absl::StatusOr<CartesianPositionData> Create(
      AnimationType animation_type, CartesianBitDepth bit_depth,
      CartesianPosition first_position);

  /*!\brief Creates CartesianPositionData with dual Cartesian positions.
   *
   * \param animation_type Animation type.
   * \param bit_depth Bit depth of coordinates (8-bit or 16-bit).
   * \param first_position First Cartesian position.
   * \param second_position Second Cartesian position.
   * \return CartesianPositionData or error if coordinates out of bounds.
   */
  static absl::StatusOr<CartesianPositionData> CreateDual(
      AnimationType animation_type, CartesianBitDepth bit_depth,
      CartesianPosition first_position, CartesianPosition second_position);

  bool friend operator==(const CartesianPositionData& lhs,
                         const CartesianPositionData& rhs) = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. Specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Returns string representation of parameter data.
   *
   * \return String representation of parameter data.
   */
  std::string ToString() const override;

  // Getters
  AnimationType animation_type() const { return animation_type_; }
  CartesianBitDepth bit_depth() const { return bit_depth_; }
  const CartesianPosition& first_position() const { return first_position_; }
  const std::optional<CartesianPosition>& second_position() const {
    return second_position_;
  }
  bool is_dual() const { return second_position_.has_value(); }

 private:
  CartesianPositionData(AnimationType animation_type,
                        CartesianBitDepth bit_depth,
                        CartesianPosition first_position,
                        std::optional<CartesianPosition> second_position)
      : ParameterData(),
        animation_type_(animation_type),
        bit_depth_(bit_depth),
        first_position_(std::move(first_position)),
        second_position_(std::move(second_position)) {}

  AnimationType animation_type_ = AnimationType::kStep;
  CartesianBitDepth bit_depth_ = CartesianBitDepth::k8Bit;
  CartesianPosition first_position_;
  std::optional<CartesianPosition> second_position_;
};

}  // namespace iamf_tools

#endif  // OBU_CARTESIAN_POSITION_DATA_H_

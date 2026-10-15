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

#ifndef OBU_POLAR_POSITION_DATA_H_
#define OBU_POLAR_POSITION_DATA_H_

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

/*!\brief Represents single 3D polar position with animated coordinates. */
struct PolarPosition {
  AnimatedParameterData<int16_t> azimuth =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int8_t> elevation =
      AnimatedParameterData<int8_t>::MakeStep(0);
  AnimatedParameterData<uint8_t> distance =
      AnimatedParameterData<uint8_t>::MakeStep(0);

  bool friend operator==(const PolarPosition& lhs,
                         const PolarPosition& rhs) = default;
};

/*!\brief Parameter data for polar positions (supports single and dual points).
 */
class PolarPositionData : public ParameterData {
 public:
  PolarPositionData() = default;

  /*!\brief Overridden destructor. */
  ~PolarPositionData() override = default;

  /*!\brief Creates PolarPositionData from a buffer.
   *
   * \param rb Buffer to read from.
   * \param is_dual Whether to read dual polar positions (two points).
   * \return Deserialized PolarPositionData or error.
   */
  static absl::StatusOr<PolarPositionData> CreateFromBuffer(ReadBitBuffer& rb,
                                                            bool is_dual);

  /*!\brief Creates PolarPositionData with a single polar position.
   *
   * \param animation_type Animation type.
   * \param first_position First polar position.
   * \return PolarPositionData or error if coordinates out of bounds.
   */
  static absl::StatusOr<PolarPositionData> Create(AnimationType animation_type,
                                                  PolarPosition first_position);

  /*!\brief Creates PolarPositionData with dual polar positions.
   *
   * \param animation_type Animation type.
   * \param first_position First polar position.
   * \param second_position Second polar position.
   * \return PolarPositionData or error if coordinates out of bounds.
   */
  static absl::StatusOr<PolarPositionData> CreateDual(
      AnimationType animation_type, PolarPosition first_position,
      PolarPosition second_position);

  bool friend operator==(const PolarPositionData& lhs,
                         const PolarPositionData& rhs) = default;

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
  const PolarPosition& first_position() const { return first_position_; }
  const std::optional<PolarPosition>& second_position() const {
    return second_position_;
  }
  bool is_dual() const { return second_position_.has_value(); }

 private:
  PolarPositionData(AnimationType animation_type, PolarPosition first_position,
                    std::optional<PolarPosition> second_position)
      : ParameterData(),
        animation_type_(animation_type),
        first_position_(std::move(first_position)),
        second_position_(std::move(second_position)) {}

  AnimationType animation_type_ = AnimationType::kStep;
  PolarPosition first_position_;
  std::optional<PolarPosition> second_position_;
};

}  // namespace iamf_tools

#endif  // OBU_POLAR_POSITION_DATA_H_

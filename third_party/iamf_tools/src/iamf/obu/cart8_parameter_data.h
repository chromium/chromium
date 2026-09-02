/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_CART8_PARAMETER_DATA_H_
#define OBU_CART8_PARAMETER_DATA_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

struct Cart8ParameterData : public ParameterData {
 public:
  Cart8ParameterData() = default;

  /*!\brief Overridden destructor.
   */
  ~Cart8ParameterData() override = default;

  /*!\brief Creates a `Cart8ParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return Deserialized `Cart8ParameterData` or error.
   */
  static absl::StatusOr<Cart8ParameterData> CreateFromBuffer(ReadBitBuffer& rb);

  /*!\brief Makes a `Cart8ParameterData`.
   *
   * \param animation_type Animation type.
   * \param x Animated coordinate x.
   * \param y Animated coordinate y.
   * \param z Animated coordinate z.
   * \return `Cart8ParameterData` object.
   */
  static Cart8ParameterData Make(AnimationType animation_type,
                                 AnimatedParameterData<int8_t> x,
                                 AnimatedParameterData<int8_t> y,
                                 AnimatedParameterData<int8_t> z);

  bool friend operator==(const Cart8ParameterData& lhs,
                         const Cart8ParameterData& rhs) = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the cart8 parameter data.
   */
  void Print() const override;

  // Getters
  AnimationType animation_type() const { return animation_type_; }
  const AnimatedParameterData<int8_t>& x() const { return x_; }
  const AnimatedParameterData<int8_t>& y() const { return y_; }
  const AnimatedParameterData<int8_t>& z() const { return z_; }

 private:
  Cart8ParameterData(AnimationType input_animation_type,
                     AnimatedParameterData<int8_t> input_x,
                     AnimatedParameterData<int8_t> input_y,
                     AnimatedParameterData<int8_t> input_z)
      : ParameterData(),
        animation_type_(input_animation_type),
        x_(input_x),
        y_(input_y),
        z_(input_z) {}

  AnimationType animation_type_ = AnimationType::kStep;
  AnimatedParameterData<int8_t> x_ = AnimatedParameterData<int8_t>::MakeStep(0);
  AnimatedParameterData<int8_t> y_ = AnimatedParameterData<int8_t>::MakeStep(0);
  AnimatedParameterData<int8_t> z_ = AnimatedParameterData<int8_t>::MakeStep(0);
};
}  // namespace iamf_tools

#endif  // OBU_CART8_PARAMETER_DATA_H_

/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_MIX_GAIN_PARAMETER_DATA_H_
#define OBU_MIX_GAIN_PARAMETER_DATA_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

struct MixGainParameterData : public ParameterData {
  /*!\brief Constructor.
   *
   * \param input_param_data Input metadata describing the animation type.
   */
  // TODO(b/549854166): Remove this once migration to `CreateFromBuffer` is
  //                     complete.
  explicit MixGainParameterData(AnimatedParameterData<int16_t> input_param_data)
      : ParameterData(), param_data(input_param_data) {}
  MixGainParameterData()
      : param_data(AnimatedParameterData<int16_t>::MakeStep(0)) {}

  /*!\brief Overridden destructor.*/
  ~MixGainParameterData() override = default;

  /*!\brief Creates a `MixGainParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return Deserialized `MixGainParameterData` or error.
   */
  static absl::StatusOr<MixGainParameterData> CreateFromBuffer(
      ReadBitBuffer& rb);

  /*!\brief Makes a `MixGainParameterData`.
   *
   * \param input_param_data Animated parameter data block.
   * \return `MixGainParameterData` object.
   */
  static MixGainParameterData Make(
      AnimatedParameterData<int16_t> input_param_data);

  /*!\brief Gets the animation type of the parameter data.
   *
   * \return Animation type.
   */
  AnimationType GetAnimationType() const;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the mix gain parameter data.
   */
  void Print() const override;

  AnimatedParameterData<int16_t> param_data;
};

}  // namespace iamf_tools

#endif  // OBU_MIX_GAIN_PARAMETER_DATA_H_

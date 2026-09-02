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
#ifndef OBU_PARAMETER_BLOCK_H_
#define OBU_PARAMETER_BLOCK_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/subblock_schedule.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A Parameter Block OBU.
 *
 * The metadata specified in this OBU defines the parameter values for an
 * algorithm for an indicated duration, including any animation of the parameter
 * values over this duration.
 *
 * Created via one of the factory methods:
 *
 *   - `CreateMode0`, when the associated param definition has
 *       `param_definition_mode` of 0.
 *   - `CreateMode1`, when the associated param definition has
 *       `param_definition_mode` of 1 and a `SubblockSchedule` is available.
 *   - `CreateFromBuffer`, from a raw binary representation.
 */
class ParameterBlockObu : public ObuBase {
 public:
  /*!\brief Peeks the parameter ID from the bitstream.
   *
   * This function does not consume any data from the bitstream.
   *
   * \param rb Buffer to read from.
   * \return Parameter ID if successful. Returns `absl::ResourceExhaustedError`
   *         if there is not enough data to read a parameter ID. Returns other
   *         errors if the bitstream is invalid.
   */
  static absl::StatusOr<DecodedUleb128> PeekParameterId(ReadBitBuffer& rb);

  /*!\brief Creates a `ParameterBlockObu` with `param_definition_mode` of 0.
   *
   * \param header `ObuHeader` of the OBU.
   * \param param_definition Parameter definition to use.
   * \return Unique pointer to a `ParameterBlockObu` on success, or `nullptr`
   *         on failure.
   */
  static std::unique_ptr<ParameterBlockObu> CreateMode0(
      const ObuHeader& header, const ParamDefinition& param_definition);

  /*!\brief Creates a `ParameterBlockObu` with `param_definition_mode` of 1.
   *
   * \param header `ObuHeader` of the OBU.
   * \param param_definition Parameter definition to use.
   * \param schedule Schedule of the parameter block.
   * \return Unique pointer to a `ParameterBlockObu` on success, or `nullptr`
   *         on failure.
   */
  static std::unique_ptr<ParameterBlockObu> CreateMode1(
      const ObuHeader& header, const ParamDefinition& param_definition,
      const SubblockSchedule& schedule);

  /*!\brief Creates a `ParameterBlockObu` from a `ReadBitBuffer`.
   *
   * The user may need to call `PeekParameterId` in order to determine the param
   * definition to pass to this function.
   *
   * This function is designed to be used from the perspective of the decoder.
   * It will call `ReadAndValidatePayload` in order to read from the buffer;
   * therefore it can fail.
   *
   * \param header `ObuHeader` of the OBU.
   * \param payload_size Size of the obu payload in bytes.
   * \param param_definition Associated param definition.
   * \param rb `ReadBitBuffer` where the `ParameterBlockObu` data is stored.
   *        Data read from the buffer is consumed.
   * \return Unique pointer to a `ParameterBlockObu` on success. A specific
   *         status on failure.
   */
  static absl::StatusOr<std::unique_ptr<ParameterBlockObu>> CreateFromBuffer(
      const ObuHeader& header, int64_t payload_size,
      const ParamDefinition& param_definition, ReadBitBuffer& rb);

  /*!\brief Destructor. */
  ~ParameterBlockObu() override = default;

  /*!\brief Interpolate the value of a `MixGainParameterData`.
   *
   * \param mix_gain_parameter_data `MixGainParameterData` to interpolate.
   * \param start_time Start time of the `MixGainParameterData`.
   * \param end_time End time of the `MixGainParameterData`.
   * \param target_time Target time to get the interpolated value of.
   * \param target_mix_gain_db Output inteprolated mix gain value in dB.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status InterpolateMixGainParameterData(
      const MixGainParameterData* mix_gain_parameter_data,
      InternalTimestamp start_time, InternalTimestamp end_time,
      InternalTimestamp target_time, float& target_mix_gain_db);

  /*!\brief Gets the duration of the parameter block.
   *
   * \return Duration of the OBU.
   */
  DecodedUleb128 GetDuration() const;

  /*!\brief Gest the constant subblock interval of the OBU.
   *
   * \return Constant subblock duration of the OBU.
   */
  DecodedUleb128 GetConstantSubblockDuration() const;

  /*!\brief Gets the number of subblocks of the OBU.
   *
   * \return Number of subblocks of the OBU.
   */
  DecodedUleb128 GetNumSubblocks() const;

  /*!\brief Gets the duration of the subblock.
   *
   * \param subblock_index Index of the subblock to get the duration of.
   * \return Duration of the subblock or `absl::InvalidArgumentError()` on
   *         failure.
   */
  absl::StatusOr<DecodedUleb128> GetSubblockDuration(int subblock_index) const;

  /*!\brief Outputs the linear mix gains at each tick starting at the start of
   * the OBU.
   *
   * \param linear_mix_gain_per_tick Output linear mix gain converted from a dB
   *        value stored as Q7.8.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` on
   *         failure.
   */
  absl::Status GetLinearMixGains(
      std::vector<float>& linear_mix_gain_per_tick) const;

  /*!\brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  // Mapped from an Audio Element or Mix Presentation OBU parameter ID.
  const DecodedUleb128 parameter_id_;

  // Length `num_subblocks_`.
  std::vector<std::unique_ptr<ParameterData>> subblocks_;

 private:
  /*!\brief Private constructor.
   *
   * \param header `ObuHeader` of the OBU.
   * \param param_definition Parameter definition.
   * \param schedule Schedule of the parameter block.
   */
  ParameterBlockObu(const ObuHeader& header,
                    const ParamDefinition& param_definition,
                    const std::optional<SubblockSchedule>& schedule);

  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*!\brief Reads the OBU payload from the buffer.
   *
   * \param payload_size Size of the obu payload in bytes.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  absl::Status ReadAndValidatePayloadDerived(int64_t payload_size,
                                             ReadBitBuffer& rb) override;

  // The schedule is conditionally included if `param_definition_mode` is
  // `kModeScheduleInParameterBlock`.
  std::optional<SubblockSchedule> schedule_;

  // Parameter definition corresponding to this parameter block.
  const ParamDefinition& param_definition_;
};

}  // namespace iamf_tools

#endif  // OBU_PARAMETER_BLOCK_H_

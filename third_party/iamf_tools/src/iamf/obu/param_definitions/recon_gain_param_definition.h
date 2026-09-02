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
#ifndef OBU_PARAM_DEFINITIONS_RECON_GAIN_PARAM_DEFINITION_H_
#define OBU_PARAM_DEFINITIONS_RECON_GAIN_PARAM_DEFINITION_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

struct ChannelNumbers {
  friend bool operator==(const ChannelNumbers& lhs,
                         const ChannelNumbers& rhs) = default;
  // Number of surround channels.
  int surround = 0;
  // Number of low-frequency effects channels.
  int lfe = 0;
  // Number of height channels.
  int height = 0;
  // Number of bottom channels.
  int bottom = 0;
};

/*!\brief Parameter definition for recon gain.
 */
class ReconGainParamDefinition : public ParamDefinition {
 public:
  /* Additional data useful for creating parameter (sub)blocks.
   *
   * Present only in some intermediate stages of encoder, decoder, and
   * transcoder and are will not be read from/written to bitstreams.
   */
  struct ReconGainAuxiliaryData {
    bool recon_gain_is_present_flag = false;
    ChannelNumbers channel_numbers_for_layer;
    friend bool operator==(const ReconGainAuxiliaryData& lhs,
                           const ReconGainAuxiliaryData& rhs) = default;
  };

  /*!\brief Constructor.
   *
   * \param base_args Arguments for `ParamDefinitionBase`.
   * \param audio_element_id ID of the Audio Element OBU that uses this
   *        parameter.
   */
  ReconGainParamDefinition(const ParamDefinition::BaseArgs& base_args,
                           uint32_t audio_element_id)
      : ParamDefinition(kParameterDefinitionReconGain, base_args),
        audio_element_id_(audio_element_id) {}

  /*!\brief Default destructor.
   */
  ~ReconGainParamDefinition() override = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override;

  /*!\brief Reads from a buffer and validates the resulting output.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb) override;

  /*!\brief Creates parameter data from a buffer.
   *
   * The created instance will be of type `ReconGainInfoParameterData`.
   *
   * \param rb Buffer to read from.
   * \return Unique pointer to created parameter data, or specific error
   *         on failure.
   */
  absl::StatusOr<std::unique_ptr<ParameterData>> CreateParameterDataFromBuffer(
      ReadBitBuffer& rb) const override;

  /*!\brief Prints the parameter definition.
   */
  void Print() const override;

  friend bool operator==(const ReconGainParamDefinition& lhs,
                         const ReconGainParamDefinition& rhs) = default;

  /*!\brief ID of the Audio Element OBU that uses this recon gain parameter.
   */
  const uint32_t audio_element_id_;

  // Vector of size equal to the number of layers in the corresponding
  // audio element.
  std::vector<ReconGainAuxiliaryData> aux_data_;
};

}  // namespace iamf_tools

#endif  // OBU_PARAM_DEFINITIONS_RECON_GAIN_PARAM_DEFINITION_H_

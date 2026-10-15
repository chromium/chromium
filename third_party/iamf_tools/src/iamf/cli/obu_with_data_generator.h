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

#ifndef CLI_OBU_WITH_DATA_GENERATOR_H_
#define CLI_OBU_WITH_DATA_GENERATOR_H_

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A collection of utility functions to generate OBUs with data.
 */
class ObuWithDataGenerator {
 public:
  /*!\brief Creates an `AudioElementWithData` from the given audio element OBU.
   *
   * \param codec_config_obus Map of Codec Config OBUs.
   * \param Audio Element OBUs to process.
   * \return Audio Element with data if the process is successful, or a specific
   *         status on failure.
   */
  static absl::StatusOr<AudioElementWithData> GenerateAudioElementWithData(
      const DescriptorObus::CodecConfigsById& codec_config_obus,
      const AudioElementObu& audio_element_obu);

  /*!\brief Creates an `AudioFrameWithData` instance.
   *
   * \param audio_element_with_data `AudioElementWithData` associated to the
   *        audio frame.
   * \param audio_frame_obu Input audio frame OBU.
   * \param global_timing_module Module to keep track of frame-by-frame
   *        timestamps.
   * \param parameters_manager Maintains the state of the parameters.
   * \return Audio frame with data if the process is successful. A specific
   *         status on failure.
   */
  static absl::StatusOr<AudioFrameWithData> GenerateAudioFrameWithData(
      const AudioElementWithData& audio_element_with_data,
      const AudioFrameObu& audio_frame_obu,
      GlobalTimingModule& global_timing_module,
      ParametersManager& parameters_manager);

  /*!\brief Creates a `ParameterBlockWithData` instance.
   *
   * \param input_start_timestamp Expected start timestamp of this parameter
   *        block to check that there is no gap in the parameter substream.
   * \param global_timing_module Module to keep track of frame-by-frame
   *        timestamps.
   * \param parameter_block_obu Unique pointer to a parameter block OBU.
   *        Ownership will be transferred to the output argument.
   * \return Parameter block with data if the process is successful. A specific
   *         status on failure.
   */
  static absl::StatusOr<ParameterBlockWithData> GenerateParameterBlockWithData(
      InternalTimestamp input_start_timestamp,
      GlobalTimingModule& global_timing_module,
      std::unique_ptr<ParameterBlockObu> parameter_block_obu);

  /*!\brief Fills substream counts for each layer of the layout config.
   *
   * Automatically populates `substream_count` and `coupled_substream_count` in
   * each layer of `config` based on the loudspeaker layouts.
   *
   * \param config Scalable channel layout config to fill in-place.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status FillSubstreamCounts(ScalableChannelLayoutConfig& config);

  /*!\brief Validates substream counts and populates metadata about the config.
   *
   * \param audio_substream_ids Ordered list of substream IDs in the OBU.
   * \param config Scalable channel layout config to validate and process.
   * \param substream_id_to_labels `audio_substream_id` to output label map.
   * \param label_to_output_gain Output param populated by this function.
   * \param channel_numbers_for_layers Output param populated by this function.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status FinalizeScalableChannelLayoutConfig(
      const std::vector<DecodedUleb128>& audio_substream_ids,
      const ScalableChannelLayoutConfig& config,
      SubstreamIdLabelsMap& substream_id_to_labels,
      LabelGainMap& label_to_output_gain,
      std::vector<ChannelNumbers>& channel_numbers_for_layers);

  /*!\brief Populates substream_id_to_labels for the ambisonics config.
   *
   * \param audio_element_obu Mono ambisonics config OBU to process.
   * \param substream_id_to_labels Output map of substream IDs to labels.
   * \return `absl::OkStatus()` on success. An error if the input OBU is not an
   *         ambisonics config. A specific status on failure.
   */
  static absl::Status FinalizeAmbisonicsConfig(
      const AudioElementObu& audio_element_obu,
      SubstreamIdLabelsMap& substream_id_to_labels);

  /*!\brief Populates substream_id_to_labels for the objects config.
   *
   * \param audio_element_obu Objects config OBU to process.
   * \param substream_id_to_labels Output map of substream IDs to labels.
   * \return `absl::OkStatus()` on success. An error if the input OBU is not an
   *         objects config. A specific status on failure.
   */
  static absl::Status FinalizeObjectsConfig(
      const AudioElementObu& audio_element_obu,
      SubstreamIdLabelsMap& substream_id_to_labels);
};
}  // namespace iamf_tools

#endif  // CLI_OBU_WITH_DATA_GENERATOR_H_

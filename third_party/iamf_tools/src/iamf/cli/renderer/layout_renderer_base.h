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

#ifndef CLI_RENDERER_LAYOUT_RENDERER_BASE_H_
#define CLI_RENDERER_LAYOUT_RENDERER_BASE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Base class for rendering audio elements to a specific layout.
 *
 * Subclasses provide different implementations for how the rendering is
 * performed.
 */
class LayoutRendererBase {
 public:
  /*!\brief Destructor. */
  virtual ~LayoutRendererBase() = default;

  /*!\brief Renders all audio elements for a given temporal unit.
   *
   * This method encapsulates the core logic of iterating through audio
   * elements, rendering them, applying mix gains, and mixing them into a
   * final output.
   *
   * \param id_to_labeled_frame Map from Audio Element ID to its labeled frame.
   * \param id_to_parameter_block Map from Parameter ID to its parameter block.
   * \param rendered_samples Output buffer for the mixed and rendered samples.
   * \param valid_rendered_samples Output span view of the valid rendered
   *        samples.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Render(
      const IdLabeledFrameMap& id_to_labeled_frame,
      const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
          id_to_parameter_block,
      std::vector<std::vector<InternalSampleType>>& rendered_samples,
      std::vector<absl::Span<const InternalSampleType>>&
          valid_rendered_samples);

 protected:
  /*!\brief Constructor.
   *
   * \param audio_element_ids Audio element IDs.
   * \param element_mix_gains Element mix gain definitions; one for each
   *        audio element.
   * \param output_mix_gain Output mix gain definition for the sub-mix.
   * \param num_channels Number of output channels for this layout.
   * \param common_sample_rate Common sample rate for the sub-mix.
   * \param common_num_samples_per_frame Common number of samples per frame.
   */
  LayoutRendererBase(
      const std::vector<DecodedUleb128>& audio_element_ids,
      const std::vector<MixGainParamDefinition>& element_mix_gains,
      const MixGainParamDefinition& output_mix_gain, int32_t num_channels,
      uint32_t common_sample_rate, uint32_t common_num_samples_per_frame);

  /*!\brief Hook called before starting loop over audio elements.
   *
   * \param id_to_labeled_frame Map from audio element ID to labeled frame.
   * \param id_to_parameter_block Map from parameter ID to parameter block.
   * \return `absl::OkStatus()` on success, or specific status on failure.
   */
  virtual absl::Status PrepareOperation(
      const IdLabeledFrameMap& id_to_labeled_frame,
      const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
          id_to_parameter_block) = 0;

  /*!\brief Hook called to process single audio element inside loop.
   *
   * \param index Index of audio element in sub-mix list.
   * \param audio_element_id ID of audio element.
   * \param labeled_frame Labeled frame of audio element.
   * \param element_mix_gain Mix gain parameter definition for audio
   *        element.
   * \param id_to_parameter_block Map from parameter ID to parameter block.
   * \return `absl::OkStatus()` on success, or specific status on failure.
   */
  virtual absl::Status PerElementOperation(
      size_t index, DecodedUleb128 audio_element_id,
      const LabeledFrame& labeled_frame,
      const MixGainParamDefinition& element_mix_gain,
      const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
          id_to_parameter_block) = 0;

  /*!\brief Hook called to finalize rendering after loop over elements.
   *
   * \param id_to_parameter_block Map from parameter ID to parameter block.
   * \param rendered_samples Output buffer for mixed and rendered samples.
   * \param valid_rendered_samples Output span view of valid rendered
   *        samples.
   * \return `absl::OkStatus()` on success, or specific status on failure.
   */
  virtual absl::Status FinalizeOperation(
      const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
          id_to_parameter_block,
      std::vector<std::vector<InternalSampleType>>& rendered_samples,
      std::vector<absl::Span<const InternalSampleType>>&
          valid_rendered_samples) = 0;

  /*!\brief Helper function to collect audio element IDs and mix gains.
   *
   * \param sub_mix_audio_elements Sub-mix audio elements containing mix gain
   *        info.
   * \param audio_element_ids Output audio element IDs.
   * \param element_mix_gains Output element mix gains.
   */
  static void CollectAudioElementIdsAndMixGains(
      const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
      std::vector<DecodedUleb128>& audio_element_ids,
      std::vector<MixGainParamDefinition>& element_mix_gains);

  const std::vector<DecodedUleb128> audio_element_ids_;
  const std::vector<MixGainParamDefinition> element_mix_gains_;
  const MixGainParamDefinition output_mix_gain_;
  const int32_t num_channels_;
  const uint32_t common_sample_rate_;
  const uint32_t common_num_samples_per_frame_;
};

}  // namespace iamf_tools

#endif  // CLI_RENDERER_LAYOUT_RENDERER_BASE_H_

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

#include "iamf/cli/renderer/layout_renderer_base.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

LayoutRendererBase::LayoutRendererBase(
    const std::vector<DecodedUleb128>& audio_element_ids,
    const std::vector<MixGainParamDefinition>& element_mix_gains,
    const MixGainParamDefinition& output_mix_gain, int32_t num_channels,
    uint32_t common_sample_rate, uint32_t common_num_samples_per_frame)
    : audio_element_ids_(audio_element_ids),
      element_mix_gains_(element_mix_gains),
      output_mix_gain_(output_mix_gain),
      num_channels_(num_channels),
      common_sample_rate_(common_sample_rate),
      common_num_samples_per_frame_(common_num_samples_per_frame) {}

absl::Status LayoutRendererBase::Render(
    const IdLabeledFrameMap& id_to_labeled_frame,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    std::vector<std::vector<InternalSampleType>>& rendered_samples,
    std::vector<absl::Span<const InternalSampleType>>& valid_rendered_samples) {
  RETURN_IF_NOT_OK(
      PrepareOperation(id_to_labeled_frame, id_to_parameter_block));

  for (size_t i = 0; i < audio_element_ids_.size(); ++i) {
    const auto audio_element_id = audio_element_ids_[i];
    const auto& element_mix_gain = element_mix_gains_[i];
    const auto& labeled_frame =
        LookupInMap(id_to_labeled_frame, audio_element_id,
                    "Labeled frame for audio element ID");
    if (!labeled_frame.ok()) {
      return labeled_frame.status();
    }
    RETURN_IF_NOT_OK(PerElementOperation(i, audio_element_id, *labeled_frame,
                                         element_mix_gain,
                                         id_to_parameter_block));
  }

  return FinalizeOperation(id_to_parameter_block, rendered_samples,
                           valid_rendered_samples);
}

void LayoutRendererBase::CollectAudioElementIdsAndMixGains(
    const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
    std::vector<DecodedUleb128>& audio_element_ids,
    std::vector<MixGainParamDefinition>& element_mix_gains) {
  audio_element_ids.reserve(sub_mix_audio_elements.size());
  element_mix_gains.reserve(sub_mix_audio_elements.size());
  for (const auto& sub_mix_audio_element : sub_mix_audio_elements) {
    audio_element_ids.emplace_back(sub_mix_audio_element.audio_element_id);
    element_mix_gains.emplace_back(sub_mix_audio_element.element_mix_gain);
  }
}

}  // namespace iamf_tools

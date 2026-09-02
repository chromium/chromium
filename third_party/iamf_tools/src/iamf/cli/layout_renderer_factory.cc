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

#include "iamf/cli/layout_renderer_factory.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/renderer/default_layout_renderer.h"
#include "iamf/cli/renderer/layout_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"

namespace iamf_tools {

std::unique_ptr<LayoutRendererBase> LayoutRendererFactory::CreateRenderer(
    const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
    const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
    const MixGainParamDefinition& output_mix_gain, const Layout& layout,
    int32_t num_channels, uint32_t common_sample_rate,
    uint32_t common_num_samples_per_frame) const {
  return DefaultLayoutRenderer::Create(
      audio_elements_in_sub_mix, sub_mix_audio_elements, output_mix_gain,
      layout, num_channels, common_sample_rate, common_num_samples_per_frame,
      RendererFactory(trimming_settings_));
}

}  // namespace iamf_tools

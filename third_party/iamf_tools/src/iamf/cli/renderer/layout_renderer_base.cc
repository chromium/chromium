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

#include <cstdint>
#include <vector>

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

}  // namespace iamf_tools

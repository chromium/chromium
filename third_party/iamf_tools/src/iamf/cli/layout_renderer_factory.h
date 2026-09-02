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

#ifndef CLI_LAYOUT_RENDERER_FACTORY_H_
#define CLI_LAYOUT_RENDERER_FACTORY_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/renderer/layout_renderer_base.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

class LayoutRendererFactory {
 public:
  /*!\brief Constructor.
   *
   * \param trimming_settings Trimming settings to configure renderers with.
   */
  explicit LayoutRendererFactory(TrimmingSettings trimming_settings)
      : trimming_settings_(trimming_settings) {}

  /*!\brief Default destructor. */
  virtual ~LayoutRendererFactory() = default;

  /*!\brief Creates a layout renderer.
   *
   * \param audio_elements_in_sub_mix Audio elements to render.
   * \param sub_mix_audio_elements Sub-mix audio elements containing rendering
   *        configuration and element mix gains.
   * \param output_mix_gain Output mix gain to apply.
   * \param layout Target layout to render to.
   * \param num_channels Number of channels in the target layout.
   * \param common_sample_rate Common sample rate of the audio.
   * \param common_num_samples_per_frame Common number of samples per frame.
   * \return Unique pointer to the created layout renderer, or `nullptr` on
   *         failure.
   */
  virtual std::unique_ptr<LayoutRendererBase> CreateRenderer(
      const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
      const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
      const MixGainParamDefinition& output_mix_gain, const Layout& layout,
      int32_t num_channels, uint32_t common_sample_rate,
      uint32_t common_num_samples_per_frame) const;

 private:
  // Cached trimming settings for generated renderers.
  const TrimmingSettings trimming_settings_;
};

}  // namespace iamf_tools

#endif  // CLI_LAYOUT_RENDERER_FACTORY_H_

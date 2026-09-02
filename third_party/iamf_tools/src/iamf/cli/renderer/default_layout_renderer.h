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

#ifndef CLI_RENDERER_DEFAULT_LAYOUT_RENDERER_H_
#define CLI_RENDERER_DEFAULT_LAYOUT_RENDERER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer/layout_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Default implementation for rendering audio elements to a layout.
 *
 * This class manages a set of audio element renderers, renders each element,
 * applies element mix gains, mixes the results, and applies the output mix
 * gain.
 */
class DefaultLayoutRenderer : public LayoutRendererBase {
 public:
  /*!\brief Creates a `DefaultLayoutRenderer`.
   *
   * NOTE: It is the user's responsibility that the `audio_elements_in_sub_mix`
   * and `audio_elements_in_sub_mix` are of the same size. A mismatch crashes
   * the program.
   *
   * \param audio_elements_in_sub_mix Pointers to the audio elements in this
   *        sub-mix.
   * \param sub_mix_audio_elements Sub-mix audio elements containing mix gain
   *        info.
   * \param output_mix_gain Output mix gain definition for the sub-mix.
   * \param layout Target layout to render to.
   * \param num_channels Number of output channels for the layout.
   * \param common_sample_rate Common sample rate of the audio.
   * \param common_num_samples_per_frame Common number of samples per frame.
   * \param renderer_factory Factory to create individual audio element
   *        renderers.
   * \return Unique pointer to the created renderer, or `nullptr` on failure.
   */
  static std::unique_ptr<DefaultLayoutRenderer> Create(
      const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
      const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
      const MixGainParamDefinition& output_mix_gain, const Layout& layout,
      int32_t num_channels, uint32_t common_sample_rate,
      uint32_t common_num_samples_per_frame,
      const RendererFactoryBase& renderer_factory);

  /*!\brief Destructor. */
  ~DefaultLayoutRenderer() override = default;

  /*!\brief Renders all audio elements for a given temporal unit.
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
      std::vector<absl::Span<const InternalSampleType>>& valid_rendered_samples)
      override;

 private:
  /*!\brief Constructor.
   *
   * \param audio_element_ids Audio element IDs.
   * \param element_mix_gains Element mix gain definitions; one for each audio
   *        element.
   * \param output_mix_gain Output mix gain definition for the sub-mix.
   * \param renderers Renderers for each audio element.
   * \param num_channels Number of output channels for this layout.
   * \param common_sample_rate Common sample rate for the sub-mix.
   * \param common_num_samples_per_frame Common number of samples per frame.
   */
  DefaultLayoutRenderer(
      const std::vector<DecodedUleb128>& audio_element_ids,
      const std::vector<MixGainParamDefinition>& element_mix_gains,
      const MixGainParamDefinition& output_mix_gain,
      std::vector<std::unique_ptr<AudioElementRendererBase>> renderers,
      int32_t num_channels, uint32_t common_sample_rate,
      uint32_t common_num_samples_per_frame);

  std::vector<std::unique_ptr<AudioElementRendererBase>> renderers_;
  std::vector<std::vector<std::vector<InternalSampleType>>>
      rendered_audio_elements_;
  std::vector<float> linear_mix_gain_per_tick_;
};

}  // namespace iamf_tools

#endif  // CLI_RENDERER_DEFAULT_LAYOUT_RENDERER_H_

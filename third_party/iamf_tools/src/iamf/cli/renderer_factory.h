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
#ifndef CLI_RENDERER_FACTORY_H_
#define CLI_RENDERER_FACTORY_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Abstract class to create renderers.
 *
 * This class will be used when rendering the loudness of a mix presentation
 * layout. The mix presentation finalizer will take in a factory and use them to
 * create a renderers. By taking in a factory the finalizer can be agnostic to
 * the collection of renderers that are being used and it what circumstances
 * they are used.
 */
class RendererFactoryBase {
 public:
  /*!\brief Creates a renderer based on the audio element and layout.
   *
   * \param audio_substream_ids Audio susbtream IDs.
   * \param substream_id_to_labels Mapping of substream IDs to labels.
   * \param audio_element_type Type of the audio element.
   * \param audio_element_config Configuration of the audio element.
   * \param rendering_config Configuration of the renderer.
   * \param loudness_layout Layout to render to.
   * \param num_samples_per_frame Number of samples per frame.
   * \param sample_rate Sample rate of the rendered output.
   * \return Unique pointer to an audio element renderer or `nullptr` if it not
   *         known how to render the audio element.
   */
  virtual std::unique_ptr<AudioElementRendererBase> CreateRendererForLayout(
      const std::vector<DecodedUleb128>& audio_substream_ids,
      const SubstreamIdLabelsMap& substream_id_to_labels,
      AudioElementObu::AudioElementType audio_element_type,
      const AudioElementObu::AudioElementConfig& audio_element_config,
      const RenderingConfig& rendering_config, const Layout& loudness_layout,
      size_t num_samples_per_frame, size_t sample_rate) const = 0;

  /*!\brief Destructor. */
  virtual ~RendererFactoryBase() = 0;
};

/*!\brief Factory which creates a renderers.
 *
 * This factory provides renderers in a best-effort manner according to the
 * recommendations in the IAMF specification (section 7.3.2). When a recommended
 * renderer is not implemented by `iamf-tools` the factory will fallback to
 * returning a `nullptr`.
 */
class RendererFactory : public RendererFactoryBase {
 public:
  /*!\brief Creates a renderer based on the audio element and layout.
   *
   * \param audio_substream_ids Audio susbtream IDs.
   * \param substream_id_to_labels Mapping of substream IDs to labels.
   * \param audio_element_type Type of the audio element.
   * \param audio_element_config Configuration of the audio element.
   * \param rendering_config Configuration of the renderer.
   * \param loudness_layout Layout to render to.
   * \param num_samples_per_frame Number of samples per frame.
   * \param sample_rate Sample rate of the rendered output.
   * \return Unique pointer to an audio element renderer or `nullptr` if it not
   *         known how to render the audio element.
   */
  std::unique_ptr<AudioElementRendererBase> CreateRendererForLayout(
      const std::vector<DecodedUleb128>& audio_substream_ids,
      const SubstreamIdLabelsMap& substream_id_to_labels,
      AudioElementObu::AudioElementType audio_element_type,
      const AudioElementObu::AudioElementConfig& audio_element_config,
      const RenderingConfig& rendering_config, const Layout& loudness_layout,
      size_t num_samples_per_frame, size_t sample_rate) const override;

  /*!\brief Constructor.
   *
   * \param trimming_settings Trimming settings to configure renderers with.
   */
  explicit RendererFactory(TrimmingSettings trimming_settings = {})
      : trimming_settings_(trimming_settings) {}

  /*!\brief Destructor. */
  ~RendererFactory() override = default;

 private:
  // Cached trimming settings to apply to each generated audio element renderer.
  const TrimmingSettings trimming_settings_;
};

}  // namespace iamf_tools

#endif  // CLI_RENDERER_FACTORY_H_

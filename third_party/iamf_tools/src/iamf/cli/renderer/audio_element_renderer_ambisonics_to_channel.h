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
#ifndef CLI_RENDERER_AUDIO_ELEMENT_RENDERER_AMBISONICS_TO_CHANNEL_H_
#define CLI_RENDERER_AUDIO_ELEMENT_RENDERER_AMBISONICS_TO_CHANNEL_H_
#include <cstddef>
#include <memory>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Renders demixed channels to the requested output layout.
 *
 * This class represents a renderer which is suitable for use for a scene-based
 * audio element being rendered to loudspeakers according to IAMF Spec 7.3.2.2
 * (https://aomediacodec.github.io/iamf/#processing-mixpresentation-rendering-a2l).
 *
 * - Call `RenderAudioFrame()` to render a labeled frame. The rendering may
 *   happen asynchronously.
 * - Call `SamplesAvailable()` to see if there are samples available.
 * - Call `Flush()` to retrieve finished frames, in the order they were
 *   received by `RenderLabeledFrame()`.
 * - Call `Finalize()` to close the renderer, telling it to finish rendering
 *   any remaining frames, which can be retrieved one last time via `Flush()`.
 *   After calling `Finalize()`, any subsequent call to `RenderAudioFrame()`
 *   may fail.
 */
class AudioElementRendererAmbisonicsToChannel
    : public AudioElementRendererBase {
 public:
  /*!\brief Creates a renderer from an ambisonics-based config.
   *
   * \param ambisonics_config Config for the ambisonics layout.
   * \param audio_substream_ids Audio substream IDs.
   * \param substream_id_to_labels Mapping of substream IDs to labels.
   * \param playback_layout Layout of the audio element to be rendered.
   * \param num_samples_per_frame Number of samples per frame.
   * \return Render to use or `nullptr` on failure.
   */
  static std::unique_ptr<AudioElementRendererAmbisonicsToChannel>
  CreateFromAmbisonicsConfig(
      const AmbisonicsConfig& ambisonics_config,
      const std::vector<DecodedUleb128>& audio_substream_ids,
      const SubstreamIdLabelsMap& substream_id_to_labels,
      const Layout& playback_layout, size_t num_samples_per_frame,
      const TrimmingSettings trimming_settings = {});

  /*!\brief Destructor. */
  ~AudioElementRendererAmbisonicsToChannel() override = default;

 private:
  /*!\brief Constructor.
   *
   * Used only by the factory method.
   *
   * \param num_output_channels Number of output channels.
   * \param ambisonics_config Config for the ambisonics layout.
   * \param ordered_labels Ordered list of channel labels to render.
   * \param gains Gains matrix.
   */
  AudioElementRendererAmbisonicsToChannel(
      size_t num_output_channels, size_t num_samples_per_frame,
      const std::vector<ChannelLabel::Label>& ordered_labels,
      const std::vector<std::vector<double>>& gains,
      const TrimmingSettings trimming_settings)
      : AudioElementRendererBase(ordered_labels, num_samples_per_frame,
                                 num_output_channels, trimming_settings),
        gains_(gains) {}

  /*!\brief Renders samples.
   *
   * \param samples_to_render Samples to render arranged in (channel, time).
   *        Rendered samples will be stored in the field `rendered_samples_`.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status RenderSamples(
      absl::Span<const absl::Span<const InternalSampleType>> samples_to_render)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) override;

  const std::vector<std::vector<double>> gains_;
};

}  // namespace iamf_tools
#endif  // CLI_RENDERER_AUDIO_ELEMENT_RENDERER_AMBISONICS_TO_CHANNEL_H_

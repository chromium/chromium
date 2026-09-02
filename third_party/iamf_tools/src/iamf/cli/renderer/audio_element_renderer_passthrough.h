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
#ifndef CLI_RENDERER_AUDIO_ELEMENT_RENDERER_PASSTHROUGH_H_
#define CLI_RENDERER_AUDIO_ELEMENT_RENDERER_PASSTHROUGH_H_
#include <cstddef>
#include <memory>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
/*!\brief Passthrough demixed channels corresponding with output layout.
 *
 * This class represents a renderer which is suitable for use when the
 * associated audio element has a layer which matches the playback layout
 * according to IAMF Spec 7.3.2.1
 * (https://aomediacodec.github.io/iamf/#processing-mixpresentation-rendering-m2l).
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
class AudioElementRendererPassThrough : public AudioElementRendererBase {
 public:
  /*!\brief Creates a passthrough renderer from a channel-based config.
   *
   * Creates a passthrough renderer if it is suitable for use according to IAMF
   * Spec 7.3.2.1. In particular when either of these conditions are true:
   *  - "If num_layers = 1, use the loudspeaker_layout of the Audio Element."
   *  - "Else, if there is an Audio Element with a loudspeaker_layout that
   * matches the playback layout, use it."
   *
   * \param scalable_channel_layout_config Config for the scalable channel
   *        layout.
   * \param playback_layout Layout of the audio element to be rendered
   * \param num_samples_per_frame Number of samples per frame.
   * \param trimming_settings Trimming settings to use.
   * \return Render to use or `nullptr` if it would not be suitable for use.
   */
  static std::unique_ptr<AudioElementRendererPassThrough>
  CreateFromScalableChannelLayoutConfig(
      const ScalableChannelLayoutConfig& scalable_channel_layout_config,
      const Layout& playback_layout, size_t num_samples_per_frame,
      const TrimmingSettings trimming_settings = {});

  /*!\brief Destructor. */
  ~AudioElementRendererPassThrough() override = default;

 private:
  /*!\brief Constructor.
   *
   * \param ordered_labels Ordered list of channel labels to render.
   */
  AudioElementRendererPassThrough(
      const std::vector<ChannelLabel::Label>& ordered_labels,
      size_t num_samples_per_frame, const TrimmingSettings trimming_settings)
      : AudioElementRendererBase(
            ordered_labels,
            // For a passthrough renderer, (number of output channels)
            // is the same as (number of input channels).
            num_samples_per_frame,
            /*num_output_channels=*/ordered_labels.size(), trimming_settings) {}

  /*!\brief Renders samples.
   *
   * \param samples_to_render Samples to render arranged in (channel, time).
   *        Rendered samples will be stored in the field `rendered_samples_`.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status RenderSamples(
      absl::Span<const absl::Span<const InternalSampleType>> samples_to_render)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_) override;
};

}  // namespace iamf_tools
#endif  // CLI_RENDERER_AUDIO_ELEMENT_RENDERER_PASSTHROUGH_H_

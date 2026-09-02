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
#ifndef CLI_RENDERER_AUDIO_ELEMENT_RENDERER_CHANNEL_TO_CHANNEL_H_
#define CLI_RENDERER_AUDIO_ELEMENT_RENDERER_CHANNEL_TO_CHANNEL_H_
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
/*!\brief Renders demixed channels to the requested output layout.
 *
 * This class represents a renderer which is suitable for use when the
 * associated audio element has a layer which does not matches the playback
 * layout according to IAMF Spec 7.3.2.1
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
class AudioElementRendererChannelToChannel : public AudioElementRendererBase {
 public:
  /*!\brief Creates a channel to channel renderer from a channel-based config.
   *
   * \param scalable_channel_layout_config Config for the scalable channel
   *        layout.
   * \param playback_layout Layout of the audio element to be rendered.
   * \param num_samples_per_frame Number of samples per frame.
   * \return Render to use or `nullptr` on failure.
   */
  static std::unique_ptr<AudioElementRendererChannelToChannel>
  CreateFromScalableChannelLayoutConfig(
      const ScalableChannelLayoutConfig& scalable_channel_layout_config,
      const Layout& playback_layout, size_t num_samples_per_frame,
      const TrimmingSettings trimming_settings = {});

  /*!\brief Destructor. */
  ~AudioElementRendererChannelToChannel() override = default;

 private:
  /*!\brief Constructor.
   *
   * Used only by the factory method.
   *
   * \param input_key Key representing the input loudspeaker layout.
   * \param output_key Key representing the output loudspeaker layout.
   * \param num_output_channels Number of output channels.
   * \param num_samples_per_frame Number of samples per frame.
   * \param ordered_labels Ordered list of channel labels to render.
   */
  AudioElementRendererChannelToChannel(
      absl::string_view input_key, absl::string_view output_key,
      size_t num_output_channels, size_t num_samples_per_frame,
      const TrimmingSettings trimming_settings,
      const std::vector<ChannelLabel::Label>& ordered_labels,
      const std::vector<std::vector<double>>& gains)
      : AudioElementRendererBase(ordered_labels, num_samples_per_frame,
                                 num_output_channels, trimming_settings),
        input_key_(input_key),
        output_key_(output_key),
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

  const std::string input_key_;
  const std::string output_key_;
  const std::vector<std::vector<double>> gains_;
};

}  // namespace iamf_tools
#endif  // CLI_RENDERER_AUDIO_ELEMENT_RENDERER_CHANNEL_TO_CHANNEL_H_

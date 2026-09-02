/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef CLI_RENDERER_AUDIO_ELEMENT_RENDERER_BINAURAL_H_
#define CLI_RENDERER_AUDIO_ELEMENT_RENDERER_BINAURAL_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/types.h"
#include "obr/audio_buffer/audio_buffer.h"
#include "obr/renderer/obr_impl.h"

namespace iamf_tools {
/*!\brief Renders channel-based or scene-based audio elements to binaural.
 *
 * This class represents a renderer which is suitable for rendering to binaural
 * headphones as described in IAMF Spec 7.3.2.3 and 7.3.2.4
 * (https://aomediacodec.github.io/iamf/#processing-mixpresentation-rendering).
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
class AudioElementRendererBinaural : public AudioElementRendererBase {
 public:
  /*!\brief Creates a channel to binaural renderer from a channel-based config.
   *
   * \param scalable_channel_layout_config Config for the scalable channel
   *        layout.
   * \param num_samples_per_frame Number of samples per frame.
   * \param sample_rate Sample rate of the rendered output.
   * \return Render to use or `nullptr` on failure.
   */
  // TODO(b/451907102): Use OBR's restrictions for `num_samples_per_frame`.
  // TODO(b/451901158): Use OBR's restrictions for `sample_rate`.
  static std::unique_ptr<AudioElementRendererBinaural>
  CreateFromScalableChannelLayoutConfig(
      const ScalableChannelLayoutConfig& scalable_channel_layout_config,
      size_t num_samples_per_frame, size_t sample_rate,
      TrimmingSettings trimming_settings = {});

  /*!\brief Creates an ambisonics to binaural renderer.
   *
   * \param ambisonics_config Config for the ambisonics.
   * \param audio_substream_ids Audio substream IDs.
   * \param substream_id_to_labels Mapping of substream IDs to labels.
   * \param num_samples_per_frame Number of samples per frame.
   * \param sample_rate Sample rate of the rendered output.
   * \return Render to use or `nullptr` on failure.
   */
  // TODO(b/451907102): Use OBR's restrictions for `num_samples_per_frame`.
  // TODO(b/451901158): Use OBR's restrictions for `sample_rate`.
  static std::unique_ptr<AudioElementRendererBinaural>
  CreateFromAmbisonicsConfig(
      const AmbisonicsConfig& ambisonics_config,
      const std::vector<DecodedUleb128>& audio_substream_ids,
      const SubstreamIdLabelsMap& substream_id_to_labels,
      size_t num_samples_per_frame, size_t sample_rate,
      TrimmingSettings trimming_settings = {});

  /*!\brief Destructor. */
  ~AudioElementRendererBinaural() override = default;

 private:
  // OBR uses float for internal computation, so samples have to be casted to
  // this type before sending to OBR.
  typedef float ObrSampleType;

  // Type for an optional demixing matrix.
  typedef std::optional<std::vector<InternalSampleType>> OptionalDemixingMatrix;

  /*!\brief Constructor.
   *
   * \param ordered_labels Ordered list of channel labels to render.
   * \param demixing_matrix Demixing matrix used to project input samples.
   *        Only active when rendering inputs in ambisonics projection mode.
   *        Pass in `std::nullopt` otherwise.
   * \param obr Instance of an OBR renderer.
   * \param num_samples_per_frame Number of samples per frame.
   */
  AudioElementRendererBinaural(
      const std::vector<ChannelLabel::Label>& ordered_labels,
      const OptionalDemixingMatrix& demixing_matrix,
      std::unique_ptr<obr::ObrImpl> obr, size_t num_samples_per_frame,
      TrimmingSettings trimming_settings);

  /*!\brief Renders samples.
   *
   * \param samples_to_render Samples to render arranged in (channel, time).
   *        Rendered samples will be stored in the field `rendered_samples_`.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status RenderSamples(
      absl::Span<const absl::Span<const InternalSampleType>> samples_to_render)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_) override;

  std::unique_ptr<obr::ObrImpl> obr_;
  obr::AudioBuffer input_buffer_;
  obr::AudioBuffer output_buffer_;

  // Only when ambisonics projection mode is used will this hold a value
  // other than `std::nullopt`.
  const OptionalDemixingMatrix demixing_matrix_;

  // Buffer to store samples projected by the demixing matrix (if it exists).
  std::vector<std::vector<InternalSampleType>> projected_samples_;
  std::vector<absl::Span<InternalSampleType>> projected_samples_spans_;
};

}  // namespace iamf_tools
#endif  // CLI_RENDERER_AUDIO_ELEMENT_RENDERER_BINAURAL_H_

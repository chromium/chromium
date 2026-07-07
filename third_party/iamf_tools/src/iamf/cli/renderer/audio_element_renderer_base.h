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
#ifndef CLI_RENDERER_AUDIO_ELEMENT_RENDERER_BASE_H_
#define CLI_RENDERER_AUDIO_ELEMENT_RENDERER_BASE_H_
#include <cstddef>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
/*!\brief Abstract class to render a demixed audio element to a playback layout.
 *
 * This class represents an abstract interface to render a single audio element
 * to a single layout according to IAMF Spec 7.3.2
 * (https://aomediacodec.github.io/iamf/#processing-mixpresentation-rendering).
 *
 * - Call `RenderAudioFrame()` to render a labeled frame. The rendering may
 *   happen asynchronously.
 * - Call `Flush()` to retrieve finished frames, in the order they were
 *   received by `RenderLabeledFrame()`.
 * - Call `Finalize()` to close the renderer, telling it to finish rendering
 *   any remaining frames. Afterwards `IsFinalized()` should be called until it
 *   returns true, then audio frames should be  retrieved one last time via
 *   `Flush()`. After calling `Finalize()`, any subsequent call to
 *   `RenderAudioFrame()` may fail.
 * - Call `IsFinalized()` to ensure the renderer is Finalized.
 */
class AudioElementRendererBase {
 public:
  /*!\brief Destructor. */
  virtual ~AudioElementRendererBase() = 0;

  /*!\brief Renders samples stored in labeled frames.
   *
   * \param labeled_frame Labeled frame to render.
   * \return Number of ticks that will be rendered. A specific status on
   *         failure.
   */
  absl::StatusOr<size_t> RenderLabeledFrame(const LabeledFrame& labeled_frame);

  /*!\brief Flushes finished audio frames.
   *
   * \param rendered_samples Vector to append rendered samples to, arranged in
   *        (channel, time) axes.
   */
  void Flush(std::vector<std::vector<InternalSampleType>>& rendered_samples);

  /*!\brief Finalizes the renderer. Waits for it to finish any remaining frames.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status Finalize() {
    absl::MutexLock lock(mutex_);
    is_finalized_ = true;
    return absl::OkStatus();
  }

  /*!\brief Checks if the renderer is finalized.
   *
   * Sub-classes should override this if the renderer is not finalized directly
   * in the body of `Finalize()`.
   *
   * \return `true` if the render is finalized. `false` otherwise.
   */
  virtual bool IsFinalized() const {
    absl::MutexLock lock(mutex_);
    return is_finalized_;
  }

  /*!\brief Sets the trimming settings for this renderer.
   *
   * \param trimming_settings Trimming configuration to use.
   */
  void SetTrimmingSettings(TrimmingSettings trimming_settings) {
    absl::MutexLock lock(mutex_);
    trimming_settings_ = trimming_settings;
  }

 protected:
  /*!\brief Constructor.
   *
   * \param ordered_labels Ordered list of channel labels to render.
   * \param num_samples_per_frame Number of samples per frame.
   * \param num_output_channels Number of output channels.
   */
  AudioElementRendererBase(absl::Span<const ChannelLabel::Label> ordered_labels,
                           size_t num_samples_per_frame,
                           size_t num_output_channels);

  /*!\brief Renders samples.
   *
   * \param samples_to_render Samples to render arranged in (channel, time).
   *        Rendered samples will be stored in the field `rendered_samples_`.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status RenderSamples(
      absl::Span<const absl::Span<const InternalSampleType>> samples_to_render)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) = 0;

  const std::vector<ChannelLabel::Label> ordered_labels_;
  const size_t num_samples_per_frame_ = 0;
  const size_t num_output_channels_;

  // Mutex to guard simultaneous access to data members.
  mutable absl::Mutex mutex_;

  // Buffer of samples to render arranged in (channel, time).
  std::vector<absl::Span<const InternalSampleType>> samples_to_render_
      ABSL_GUARDED_BY(mutex_);

  // Buffer of rendered samples arranged in (channel, time).
  std::vector<std::vector<InternalSampleType>> rendered_samples_
      ABSL_GUARDED_BY(mutex_);

  // Buffer storing zeros. All omitted channels' spans point to this.
  const std::vector<InternalSampleType> kEmptyChannel;

  bool is_finalized_ ABSL_GUARDED_BY(mutex_) = false;
  const LabeledFrame* current_labeled_frame_ ABSL_GUARDED_BY(mutex_) = nullptr;
  // Determines whether frame start/end sample trimming is applied when
  // arranging samples.
  TrimmingSettings trimming_settings_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace iamf_tools
#endif  // CLI_RENDERER_AUDIO_ELEMENT_RENDERER_BASE_H_

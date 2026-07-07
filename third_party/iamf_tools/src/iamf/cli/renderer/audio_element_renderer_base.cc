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
#include "iamf/cli/renderer/audio_element_renderer_base.h"

#include <cstddef>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

AudioElementRendererBase::AudioElementRendererBase(
    absl::Span<const ChannelLabel::Label> ordered_labels,
    const size_t num_samples_per_frame, const size_t num_output_channels)
    : ordered_labels_(ordered_labels.begin(), ordered_labels.end()),
      num_samples_per_frame_(num_samples_per_frame),
      num_output_channels_(num_output_channels),
      samples_to_render_(ordered_labels_.size()),
      rendered_samples_(num_output_channels),
      kEmptyChannel(num_samples_per_frame_, 0.0) {}

AudioElementRendererBase::~AudioElementRendererBase() {}

absl::StatusOr<size_t> AudioElementRendererBase::RenderLabeledFrame(
    const LabeledFrame& labeled_frame) {
  absl::MutexLock lock(mutex_);

  size_t num_valid_samples = 0;
  RETURN_IF_NOT_OK(iamf_tools::ArrangeSamples(
      labeled_frame, ordered_labels_, kEmptyChannel, trimming_settings_,
      samples_to_render_, num_valid_samples));

  // Render samples in concrete subclasses.
  current_labeled_frame_ = &labeled_frame;
  RETURN_IF_NOT_OK(RenderSamples(absl::MakeConstSpan(samples_to_render_)));

  return num_valid_samples;
}

void AudioElementRendererBase::Flush(
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  absl::MutexLock lock(mutex_);

  // Append samples in each channel of `rendered_samples_` to the corresponding
  // channel of the output `rendered_samples`.
  rendered_samples.resize(rendered_samples_.size());
  for (size_t c = 0; c < rendered_samples_.size(); c++) {
    rendered_samples[c].insert(rendered_samples[c].end(),
                               rendered_samples_[c].begin(),
                               rendered_samples_[c].end());
    rendered_samples_[c].clear();
  }
}

}  // namespace iamf_tools

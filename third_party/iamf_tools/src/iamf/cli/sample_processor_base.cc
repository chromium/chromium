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
#include "iamf/cli/sample_processor_base.h"

#include <cstddef>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

SampleProcessorBase::~SampleProcessorBase() {}

absl::Status SampleProcessorBase::PushFrame(
    absl::Span<const absl::Span<const InternalSampleType>>
        channel_time_samples) {
  if (state_ != State::kTakingSamples) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Do not use PushFrame() after Flush() is called. State= ", state_));
  }

  // Check the shape of the input data.
  RETURN_IF_NOT_OK(ValidateEqual(channel_time_samples.size(), num_channels_,
                                 "number of channels"));
  const size_t num_ticks =
      channel_time_samples.empty() ? 0 : channel_time_samples[0].size();
  for (size_t c = 0; c < num_channels_; c++) {
    RETURN_IF_NOT_OK(ValidateEqual(channel_time_samples[c].size(), num_ticks,
                                   "number of samples per channel"));
    if (channel_time_samples[c].size() > max_input_samples_per_frame_) {
      return absl::InvalidArgumentError(
          absl::StrCat("Too many samples per frame. ",
                       "The maximum number of samples per frame is: ",
                       max_input_samples_per_frame_,
                       ". The number of samples per frame received is: ",
                       channel_time_samples[c].size()));
    }
    output_channel_time_samples_[c].resize(0);
  }

  return PushFrameDerived(channel_time_samples);
}

absl::Status SampleProcessorBase::Flush() {
  if (state_ == State::kFlushCalled) {
    return absl::FailedPreconditionError(
        "Flush() called in unexpected state. Do not call Flush() twice.");
  }

  state_ = State::kFlushCalled;
  for (size_t c = 0; c < num_channels_; c++) {
    output_channel_time_samples_[c].resize(0);
  }
  return FlushDerived();
}

absl::Span<const absl::Span<const InternalSampleType>>
SampleProcessorBase::GetOutputSamplesAsSpan() {
  for (size_t c = 0; c < output_channel_time_samples_.size(); c++) {
    output_span_buffer_[c] =
        absl::MakeConstSpan(output_channel_time_samples_[c]);
  }

  return absl::MakeSpan(output_span_buffer_);
}

}  // namespace iamf_tools

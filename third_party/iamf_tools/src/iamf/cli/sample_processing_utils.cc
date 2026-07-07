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

#include "iamf/cli/sample_processing_utils.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

// Returns the common number of time ticks to be rendered for the requested
// labels or associated demixed label in `labeled_frame`. This represents the
// number of time ticks in the rendered audio after trimming.
absl::StatusOr<size_t> GetCommonNumTrimmedTimeTicks(
    const LabeledFrame& labeled_frame,
    absl::Span<const ChannelLabel::Label> ordered_labels,
    absl::Span<const InternalSampleType> empty_channel,
    TrimmingSettings trimming_settings) {
  std::optional<size_t> num_raw_time_ticks;
  for (const auto& label : ordered_labels) {
    if (label == ChannelLabel::kOmitted) {
      continue;
    }

    absl::Span<const InternalSampleType> samples_to_render;
    RETURN_IF_NOT_OK(FindSamplesOrDemixedSamples(
        label, labeled_frame.label_to_samples, samples_to_render));

    if (!num_raw_time_ticks.has_value()) {
      num_raw_time_ticks = samples_to_render.size();
    } else if (*num_raw_time_ticks != samples_to_render.size()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "All labels must have the same number of samples ", label, " (",
          samples_to_render.size(), " vs. ", *num_raw_time_ticks, ")"));
    }
  }
  if (!num_raw_time_ticks.has_value()) {
    return absl::InvalidArgumentError("No matching channels found.");
  }

  if (empty_channel.size() < *num_raw_time_ticks) {
    return absl::InvalidArgumentError(absl::StrCat(
        "`empty_channel` should contain at least as many samples as other "
        "labels: (",
        empty_channel.size(), " < ", *num_raw_time_ticks, ")"));
  }

  const uint32_t samples_to_trim_at_start =
      trimming_settings.trim_beginning ? labeled_frame.samples_to_trim_at_start
                                       : 0;
  const uint32_t samples_to_trim_at_end =
      trimming_settings.trim_end ? labeled_frame.samples_to_trim_at_end : 0;

  uint32_t total_samples_to_trim;
  RETURN_IF_NOT_OK(AddUint32CheckOverflow(
      samples_to_trim_at_start, samples_to_trim_at_end, total_samples_to_trim));
  if (*num_raw_time_ticks < total_samples_to_trim) {
    return absl::InvalidArgumentError(
        absl::StrCat("Not enough samples to render samples",
                     ". #Raw samples: ", *num_raw_time_ticks,
                     ", samples to trim at start: ", samples_to_trim_at_start,
                     ", samples to trim at end: ", samples_to_trim_at_end));
  }

  return *num_raw_time_ticks - samples_to_trim_at_start -
         samples_to_trim_at_end;
}

}  // namespace

absl::Status ArrangeSamples(
    const LabeledFrame& labeled_frame,
    absl::Span<const ChannelLabel::Label> ordered_labels,
    absl::Span<const InternalSampleType> empty_channel,
    TrimmingSettings trimming_settings,
    std::vector<absl::Span<const InternalSampleType>>& samples_to_render,
    size_t& num_valid_ticks) {
  if (ordered_labels.empty()) {
    return absl::OkStatus();
  }

  const auto common_num_trimmed_time_ticks = GetCommonNumTrimmedTimeTicks(
      labeled_frame, ordered_labels, empty_channel, trimming_settings);
  if (!common_num_trimmed_time_ticks.ok()) {
    return common_num_trimmed_time_ticks.status();
  }
  num_valid_ticks = *common_num_trimmed_time_ticks;

  const uint32_t samples_to_trim_at_start =
      trimming_settings.trim_beginning ? labeled_frame.samples_to_trim_at_start
                                       : 0;

  const auto num_channels = ordered_labels.size();
  for (size_t c = 0; c < num_channels; ++c) {
    const auto& channel_label = ordered_labels[c];
    absl::Span<const InternalSampleType> channel_samples;

    if (channel_label == ChannelLabel::kOmitted) {
      // Missing channels for mixed-order ambisonics representation will not be
      // updated. Point to the passed-in empty channel.
      channel_samples = absl::MakeConstSpan(empty_channel);
    } else {
      RETURN_IF_NOT_OK(FindSamplesOrDemixedSamples(
          channel_label, labeled_frame.label_to_samples, channel_samples));
    }

    // Return the valid portion after trimming.
    samples_to_render[c] = channel_samples.subspan(
        samples_to_trim_at_start, *common_num_trimmed_time_ticks);
  }

  return absl::OkStatus();
}

absl::Status WritePcmSample(uint32_t sample, uint8_t sample_size,
                            bool big_endian, uint8_t* const buffer,
                            size_t& write_position) {
  // Validate assumptions of the logic in the `for` loop below.
  if (sample_size % 8 != 0 || sample_size > 32) [[unlikely]] {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid sample size: ", sample_size));
  }

  for (int shift = 32 - sample_size; shift < 32; shift += 8) {
    uint8_t byte = 0;
    if (big_endian) [[unlikely]] {
      byte = (sample >> ((32 - sample_size) + (32 - (shift + 8)))) & 0xff;
    } else {
      byte = (sample >> shift) & 0xff;
    }
    buffer[write_position++] = byte;
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools

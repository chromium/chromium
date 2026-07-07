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

#ifndef CLI_SAMPLE_PROCESSING_UTILS_H_
#define CLI_SAMPLE_PROCESSING_UTILS_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Arranges the input samples to render.
 *
 * \param labeled_frame Input labeled frame.
 * \param ordered_labels Arrangement of the output channels.
 * \param empty_channel Default samples to use when the requested channel
 *        is not found.
 * \param trimming_settings Settings to enable/disable trimming at start/end.
 * \param samples_to_render Output samples to render in (channel, time) axes.
 *        Samples which should be trimmed are omitted from the output.
 * \param num_valid_ticks Number of valid time ticks in the returned
 *        `samples_to_render`, which is the length of the time-axis.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status ArrangeSamples(
    const LabeledFrame& labeled_frame,
    absl::Span<const ChannelLabel::Label> ordered_labels,
    absl::Span<const InternalSampleType> empty_channel,
    TrimmingSettings trimming_settings,
    std::vector<absl::Span<const InternalSampleType>>& samples_to_render,
    size_t& num_valid_ticks);

/*!\brief Writes the input PCM sample to a buffer.
 *
 * Writes the most significant `sample_size` bits of `sample` starting at
 * `buffer[write_position]`. It is up to the user to ensure the buffer is valid.
 *
 * \param sample Sample to write the upper `sample_size` bits of.
 * \param sample_size Sample size in bits. MUST be one of {8, 16, 24, 32}.
 * \param big_endian `true` to write the sample as big endian. `false` to write
 *        it as little endian.
 * \param buffer Start of the buffer to write to.
 * \param write_position Offset of the buffer to write to. Incremented to one
 *        after the last byte written on success.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
 *         `sample_size` is invalid.
 */
absl::Status WritePcmSample(uint32_t sample, uint8_t sample_size,
                            bool big_endian, uint8_t* buffer,
                            size_t& write_position);

/*!\brief Arranges the input samples by channel and time.
 *
 * \param samples Interleaved samples to arrange.
 * \param num_channels Number of channels.
 * \param output Output vector to write the samples to. If the number of
 *        input samples do not fill the entire output vector, the time axis will
 *        be modified to fit the actual length.
 * \param transform_samples Function to transform each sample to the output
 *        type. Default to an identity transform that always returns OK.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if the
 *         number of samples is not a multiple of the number of channels. An
 *         error propagated from `transform_samples` if it fails.
 */
template <typename InputType, typename OutputType>
absl::Status ConvertInterleavedToChannelTime(
    absl::Span<const InputType> samples, size_t num_channels,
    std::vector<std::vector<OutputType>>& output,
    const absl::AnyInvocable<absl::Status(InputType, OutputType&) const>&
        transform_samples = [](InputType in, OutputType& out) -> absl::Status {
      out = in;
      return absl::OkStatus();
    }) {
  if (samples.size() % num_channels != 0) [[unlikely]] {
    return absl::InvalidArgumentError(absl::StrCat(
        "Number of samples must be a multiple of the number of "
        "channels. Found ",
        samples.size(), " samples and ", num_channels, " channels."));
  }

  output.resize(num_channels);
  const auto num_ticks = samples.size() / num_channels;
  for (size_t c = 0; c < num_channels; ++c) {
    auto& output_for_channel = output[c];
    output_for_channel.resize(num_ticks);
    for (size_t t = 0; t < num_ticks; ++t) {
      const auto status = transform_samples(samples[t * num_channels + c],
                                            output_for_channel[t]);
      if (!status.ok()) [[unlikely]] {
        return status;
      }
    }
  }
  return absl::OkStatus();
}

/*!\brief Interleaves the input samples.
 *
 * \param samples Samples in (channel, time) axes to arrange.
 * \param output Output vector to write the interleaved samples to.
 * \param transform_samples Function to transform each sample to the output
 *        type. Default to an identity transform that always returns OK.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if the
 *         input has an inconsistent number of channels. An error propagated
 *         from `transform_samples` if it fails.
 */
template <typename InputType, typename OutputType>
absl::Status ConvertChannelTimeToInterleaved(
    absl::Span<const absl::Span<const InputType>> input,
    std::vector<OutputType>& output,
    const absl::AnyInvocable<absl::Status(InputType, OutputType&) const>&
        transform_samples = [](InputType in, OutputType& out) -> absl::Status {
      out = in;
      return absl::OkStatus();
    }) {
  const size_t num_ticks = input.empty() ? 0 : input[0].size();
  if (!std::all_of(input.begin(), input.end(), [&](const auto& channel) {
        return channel.size() == num_ticks;
      })) {
    return absl::InvalidArgumentError(
        "All channels must have the same number of ticks.");
  }

  const auto num_channels = input.size();
  output.resize(num_channels * num_ticks);
  for (size_t c = 0; c < num_channels; ++c) {
    const auto& input_for_channel = input[c];
    for (size_t t = 0; t < num_ticks; ++t) {
      OutputType transformed_sample;
      const auto status =
          transform_samples(input_for_channel[t], transformed_sample);
      if (!status.ok()) {
        return status;
      }
      output[t * num_channels + c] = transformed_sample;
    }
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools

#endif  // CLI_SAMPLE_PROCESSING_UTILS_H_

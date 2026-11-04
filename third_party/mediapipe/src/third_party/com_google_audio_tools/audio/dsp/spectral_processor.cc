/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/spectral_processor.h"

#include <algorithm>
#include <vector>

#include "audio/dsp/number_util.h"
#include "absl/types/span.h"
#include "third_party/eigen3/Eigen/Core"
#include "glog/logging.h"

namespace audio_dsp {

using ::absl::Span;
using ::Eigen::ArrayXf;
using ::Eigen::ArrayXXcf;
using ::Eigen::ArrayXXf;
using ::Eigen::Map;
using ::std::vector;

// Notes on input padding computation:
// The goal is to ensure that for any (pre-specified) input block size, we can
// get an output block that is of equal size. Since some minimal number of
// input samples is needed to process the first block, it naturally follows that
// some latency must be added meet this criterion.
// The number of output samples produced is a function of the number of
// processed blocks, n.
//   output_produced = n * hop_size.
// In order to have processed n blocks, we must have seen cumulatively at least
// block_size + hop_size * (n - 1) samples.
//   pad + input_seen >= block_size + hop_size * (n - 1),
// where pad is the unknown amount of zero padding. Combining this with our
// goal of input_seen == output_returned, and an ability to buffer output,
// output_returned <= output_produced, we get the following:
//   input_seen <= n * hop_size <= pad + input_seen - block_size - hop_size.
//
// The left side implies a derivation of n,
//   n = ceil(input_seen / hop_size),
// and the right side implies a lower bound on the amount of padding we need.
//   n * hop_size - input_seen + block_size - hop_size <= pad
//   ceil(input_seen / hop_size) * hop_size - input_seen + block_size - hop_size
//       <= pad
//   (input_seen % hop_size) + block_size - hop_size <= pad.
//
// From the lower bound, we get `pad` is the maximum of
//   pad = block_size - hop_size + (max { input_seen % hop_size })
// where the max is over all possible values of "input_seen". Suppose that
// input_seen is a sum of s1, s2, and so on of allowed input sizes. Then
// input_seen = m1*s1 + m2*s2 + ... for some integers m1, m2, ... >= 0. We now
// maximize over all values of m1 and m2.
//   pad = block_size - hop_size + (
//       max_(m1,m2,...) {(m1*s1 + m2*s2 + ...) % hop_size})
//
// We now re-express the size of our sample chunks as groupings of size D, where
// D is gcd(hop_size, s1, s2, ...), allowing us to write
// hop_size = h*D, s1 = t1*D, s2 = t2*D, etc.
//   pad = block_size - h*D +
//           (max_(m1,m2,...) {((m1*t1 + m2*t2 + ...) * D) % (h*D)})
//       = block_size - h*D +
//           (max_(m1,m2,...) {(m1*t1 + m2*t2 + ...) % h}) * D.
//
// We now assume a worst case value of max(... % h) to be h-1. The equation
// reduces dramatically:
//   pad = block_size - h*D + (h - 1) * D
//       = block_size - D.
//       = block_size - gcd(hop_size, s1, s2, ...).

namespace {
static int GreatestCommonDivisorWithHopSize(std::vector<int> all_sizes,
                                            int hop_size) {
  all_sizes.push_back(hop_size);
  return GreatestCommonDivisor(all_sizes);
}
}  // namespace

SpectralProcessorParams MakeParamsFromDeprecatedArgs(Span<const float> window,
                                                     int hop_size) {
  SpectralProcessorParams params(window.size());
  params.hop_size = hop_size;
  std::copy_n(window.begin(), window.size(), params.window.begin());
  return params;
}

SpectralProcessor::SpectralProcessor(int num_in_channels, int num_out_channels,
                                     int chunk_length, Span<const float> window,
                                     int block_length, int hop_size,
                                     Callback* block_processor)
    : SpectralProcessor(num_in_channels, num_out_channels,
                        std::vector<int>{chunk_length}, window, block_length,
                        hop_size, block_processor) {}

SpectralProcessor::SpectralProcessor(
    int num_in_channels, int num_out_channels,
    const std::vector<int>& possible_chunk_lengths, Span<const float> window,
    int block_length, int hop_size, Callback* block_processor)
    : SpectralProcessor(
          num_in_channels, num_out_channels, possible_chunk_lengths,
          MakeParamsFromDeprecatedArgs(window, hop_size), block_processor) {
  ABSL_CHECK_EQ(block_length, window.size());
}

SpectralProcessor::SpectralProcessor(
    int num_in_channels, int num_out_channels,
    const std::vector<int>& possible_chunk_lengths,
    const SpectralProcessorParams& params, Callback* block_processor)
    : num_in_channels_(num_in_channels),
      num_out_channels_(num_out_channels),
      hop_size_(params.hop_size),
      block_length_(params.block_length),
      fft_length_(params.fft_length),
      max_chunk_length_(*std::max_element(possible_chunk_lengths.begin(),
                                          possible_chunk_lengths.end())),
      block_processor_(block_processor),
      position_in_output_(0),
      // See notes above. This latency computation has a simple result, but
      // the derivation is a bit complicated.
      initial_delay_(block_length_ -
                     GreatestCommonDivisorWithHopSize(possible_chunk_lengths,
                                                      params.hop_size)),
      out_overflow_(Eigen::ArrayXXf::Zero(num_out_channels_,
                                          (max_chunk_length_ + block_length_))),
      window_(Eigen::ArrayXf::Zero(fft_length_)),
      transformer_(fft_length_, true),
      time_workspace_(ArrayXXf::Zero(
          fft_length_, std::max(num_in_channels_, num_out_channels))),
      unprocessed_block_workspace_(
          ArrayXXf::Zero(num_in_channels, fft_length_)),
      processed_block_workspace_(
          ArrayXXf::Zero(num_out_channels_, fft_length_)),
      in_fft_workspace_(RowMajorArrayXXcf::Zero(
          num_in_channels_, transformer_.GetTransformedSize())),
      out_fft_workspace_(RowMajorArrayXXcf::Zero(
          num_out_channels_, transformer_.GetTransformedSize())) {
  ABSL_CHECK_GT(num_in_channels_, 0);
  ABSL_CHECK_GT(block_length_, 0);
  ABSL_CHECK_GE(params.window.size(), block_length_);
  ABSL_CHECK_GE(fft_length_, block_length_);
  ABSL_CHECK_GT(max_chunk_length_, 0);
  ABSL_CHECK(block_processor_);
  // Make a (possibly) zero-padded window.
  std::copy_n(params.window.begin(), block_length_, window_.begin());
  Reset();
}

SpectralProcessor::SpectralProcessor(int num_in_channels, int num_out_channels,
                                     int chunk_length,
                                     const SpectralProcessorParams& params,
                                     Callback* block_processor)
    : SpectralProcessor(num_in_channels, num_out_channels,
                        std::vector<int>{chunk_length}, params,
                        block_processor) {}

void SpectralProcessor::Reset() {
  out_overflow_.setZero();
  time_workspace_.setZero();
  unprocessed_block_workspace_.setZero();
  processed_block_workspace_.setZero();
  in_fft_workspace_.setZero();
  out_fft_workspace_.setZero();
  in_circular_buffer_.Init(num_in_channels_ *
                           (max_chunk_length_ + block_length_));

  position_in_output_ = 0;

  int initial_delay_samples = num_in_channels_ * initial_delay_;
  vector<float> delay(initial_delay_samples);
  in_circular_buffer_.Write(Span<float>(delay.data(), delay.size()));
}

void SpectralProcessor::ProcessChunk(const Map<const ArrayXXf>& input,
                                     Map<ArrayXXf> output) {
  ABSL_DCHECK_EQ(input.rows(), num_in_channels());
  // This is not as strict of a check as verifying that the element is in
  // possible_chunk_lengths. If the input requirement is violated in a
  // problematic way, the ABSL_CHECK_GE(position_in_output_, 0) check below will
  // fail.
  ABSL_DCHECK_LE(input.cols(), max_chunk_length());
  ABSL_DCHECK_EQ(output.rows(), num_out_channels());
  ABSL_DCHECK_EQ(output.cols(), input.cols());
  const int this_chunk_length = input.cols();
  output.setZero();
  MoveOverflowIntoOutput(output);

  in_circular_buffer_.Write(
      Span<const float>(input.data(), num_in_channels_ * this_chunk_length));
  const int samples_to_read = num_in_channels_ * block_length_;
  while (in_circular_buffer_.NumReadableEntries() >= samples_to_read) {
    // If there is zero padding, the final portion of the unprocessed block will
    // be zero. We zero the block as a safety measure, but nothing in the
    // code actually writes nonzero entries to this portion.
    unprocessed_block_workspace_.setZero();
    in_circular_buffer_.Peek(
        Span<float>(unprocessed_block_workspace_.data(), samples_to_read));
    in_circular_buffer_.Advance(num_in_channels_ * hop_size_);

    // The returned block has already trimmed off the zero padding.
    const Map<const ArrayXXf> processed_block =
        WindowAndProcessInFrequencyDomain(unprocessed_block_workspace_);

    MoveProcessedIntoOutputAndOverflow(processed_block, position_in_output_,
                                       output);
    position_in_output_ += hop_size_;
    // We have processed enough input to be able to generate some output.
    if (position_in_output_ >= this_chunk_length) {
      break;
    }
  }
  // Adjust our counter to account for the samples produced.
  position_in_output_ -= this_chunk_length;
  ABSL_CHECK_GE(position_in_output_, 0);
}

const Map<const ArrayXXf> SpectralProcessor::WindowAndProcessInFrequencyDomain(
    const ArrayXXf& input) {
  ABSL_CHECK_EQ(input.rows(), num_in_channels());
  ABSL_CHECK_EQ(input.cols(), fft_length_);

  // Window and transform the input.
  Map<ArrayXXf> input_block(time_workspace_.data(), fft_length_,
                            num_in_channels());
  input_block = input.transpose();

  input_block.colwise() *= window_;

  for (int i = 0; i < num_in_channels_; ++i) {
    transformer_.ForwardTransform(input_block.col(i).data(),
                                  in_fft_workspace_.row(i).data());
  }
  block_processor_->ProcessSTFTBlock(in_fft_workspace_,
                                     transformer_.GetTransformedSize(),
                                     &out_fft_workspace_);

  // Transform and window the output.
  Map<ArrayXXf> output_block(time_workspace_.data(), fft_length_,
                             num_out_channels());
  for (int i = 0; i < num_out_channels_; ++i) {
    transformer_.InverseTransform(out_fft_workspace_.row(i).data(),
                                  output_block.col(i).data());
  }
  output_block.colwise() *= window_;
  // Interleave (transpose) and return the audio to the buffer.
  processed_block_workspace_ = output_block.transpose();
  return Map<const ArrayXXf>(processed_block_workspace_.data(),
                             num_out_channels(), block_length_);
}

void SpectralProcessor::MoveOverflowIntoOutput(Map<ArrayXXf> out_chunk) {
  // Copy overlap into output buffer and clear the rest.
  const int frames_to_copy =
      std::min<int>(out_overflow_.cols(), out_chunk.cols());
  out_chunk.leftCols(frames_to_copy) = out_overflow_.leftCols(frames_to_copy);
  // Shift remaining overlap left.
  const int shift = frames_to_copy;
  for (int i = 0; i < out_overflow_.cols() - shift; ++i) {
    out_overflow_.col(i) = out_overflow_.col(i + shift);
    out_overflow_.col(i + shift).setZero();
  }
}

void SpectralProcessor::MoveProcessedIntoOutputAndOverflow(
    Map<const ArrayXXf> processed, int start_frame, Map<ArrayXXf> out_chunk) {
  // Copy the processed samples into the output.
  int frames_to_copy = std::max<int>(
      0, std::min<int>(processed.cols(), out_chunk.cols() - start_frame));
  out_chunk.middleCols(start_frame, frames_to_copy) +=
      processed.leftCols(frames_to_copy);

  // When we run out of room, put samples into the overflow.
  int overflow_frames = processed.cols() - frames_to_copy;
  out_overflow_.leftCols(overflow_frames) +=
      processed.middleCols(frames_to_copy, overflow_frames);
  // Clear remaining overflow.
  out_overflow_.rightCols(out_overflow_.cols() - overflow_frames).setZero();
}

}  // namespace audio_dsp

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
#include <cmath>
#include <cstdlib>
#include <random>
#include <vector>

#include "audio/dsp/signal_generator.h"
#include "audio/dsp/testing_util.h"
#include "audio/dsp/window_functions.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

namespace audio_dsp {

using ::Eigen::ArrayXf;
using ::Eigen::ArrayXXf;

namespace {

Eigen::ArrayXXf MakeRamp(int num_channels, int length) {
  Eigen::ArrayXXf ramp(num_channels, length);
  for (int channel = 0; channel < num_channels; ++channel) {
    for (int i = 0; i < length; ++i) {
      ramp(channel, i) = i;
    }
  }
  return ramp;
}

// Fills the output with input from a different channel whenever there are more
// outputs than inputs. Otherwise, copies inputs into available outputs.
class BypassCallback : public audio_dsp::SpectralProcessor::Callback {
 public:
  BypassCallback() : block_num_(0) {}

  void ProcessSTFTBlock(
      const SpectralProcessor::RowMajorArrayXXcf& in_block,
      int num_time_domain_frames,
      SpectralProcessor::RowMajorArrayXXcf* out_block) override {
    int in_channel = 0;
    for (int out_channel = 0; out_channel < out_block->rows(); ++out_channel) {
      out_block->row(out_channel) = in_block.row(in_channel);
      ++in_channel;
      in_channel = in_channel % in_block.rows();
    }
    ++block_num_;
  }

  int block_num() { return block_num_; }

 private:
  int block_num_;
};

// Notes the bin with the maximum energy (and the energy value) per channel.
class FftCheckerCallback : public audio_dsp::SpectralProcessor::Callback {
 public:
  FftCheckerCallback() {}

  void ProcessSTFTBlock(
      const SpectralProcessor::RowMajorArrayXXcf& in_block,
      int num_time_domain_frames,
      SpectralProcessor::RowMajorArrayXXcf* out_block) override {
    max_bin_rads_per_sample_.resize(in_block.rows());
    max_bin_magnitude_.resize(in_block.rows());
    for (int i = 0; i < in_block.rows(); ++i) {
      int max_bin = 0;
      float max_magnitude = 0;
      for (int bin = 0; bin < in_block.cols(); ++bin) {
        float new_value = std::abs(in_block(i, bin));
        if (new_value > max_magnitude) {
          max_bin = bin;
          max_magnitude = new_value;
        }
      }
      max_bin_rads_per_sample_[i] =
          M_PI * max_bin / static_cast<float>(in_block.cols());
      max_bin_magnitude_[i] = max_magnitude;
    }
  }

  float max_bin_rads_per_sample(int i) { return max_bin_rads_per_sample_[i]; }
  float max_bin_magnitude(int i) { return max_bin_magnitude_[i]; }

 private:
  std::vector<float> max_bin_rads_per_sample_;
  std::vector<float> max_bin_magnitude_;
};

SpectralProcessorParams RectangularWindowNoOverlap(int block_size) {
  SpectralProcessorParams params(block_size);
  params.block_length = block_size;
  params.fft_length = block_size;
  params.hop_size = block_size;
  params.window.assign(block_size, 1.0f);
  return params;
}

// Identity window for |overlap = block_length / 2|. Window is applied twice
// to each block and the result is summed. (sqrt(0.5) ^ 2) * 2 = 1.
SpectralProcessorParams RectangularWindowHalfOverlap(int block_size) {
  SpectralProcessorParams params = RectangularWindowNoOverlap(block_size);
  params.hop_size /= 2;
  params.window.assign(block_size, std::sqrt(0.5f));
  return params;
}

// Identity window for |overlap = block_length / 4|. Window is applied twice
// to each block and the result is summed. (sqrt(0.25) ^ 2) * 4 = 1.
SpectralProcessorParams RectangularWindowQuarterOverlap(int block_size) {
  SpectralProcessorParams params = RectangularWindowNoOverlap(block_size);
  params.hop_size /= 4;
  params.window.assign(block_size, std::sqrt(0.25f));
  return params;
}

}  // namespace

// Makes sure the samples just get passed from input to output.
TEST(SpectralProcessorTest, WindowlessNoOverlapTest) {
  const int kChannels = 3;
  const int kChunkLength = 64;
  const int kBlockLength = 4;
  BypassCallback bypass_callback;

  SpectralProcessor processor(kChannels, kChannels, kChunkLength,
                              RectangularWindowNoOverlap(kBlockLength),
                              &bypass_callback);
  Eigen::ArrayXXf in_buffer =
      Eigen::ArrayXXf::Constant(kChannels, kChunkLength, 2.0f);
  Eigen::ArrayXXf out_buffer =
      Eigen::ArrayXXf::Constant(kChannels, kChunkLength, -1.0f);

  processor.ProcessChunk(in_buffer, &out_buffer);

  EXPECT_THAT(out_buffer, EigenArrayNear(Eigen::ArrayXXf::Constant(
                                             kChannels, kChunkLength, 2.0f),
                                         1e-5));
  EXPECT_EQ(kChunkLength / kBlockLength, bypass_callback.block_num());
}

TEST(SpectralProcessorTest, FiftyPercentOverlapRectangularWindowTest) {
  const int kChunkLength = 128;
  const int kBlockLength = 32;
  BypassCallback bypass_callback;
  SpectralProcessorParams params = RectangularWindowHalfOverlap(kBlockLength);

  SpectralProcessor processor(1, 1, kChunkLength, params, &bypass_callback);
  Eigen::ArrayXXf in_buffer = Eigen::ArrayXXf::Constant(1, kChunkLength, 2.0f);
  Eigen::ArrayXXf out_buffer =
      Eigen::ArrayXXf::Constant(1, kChunkLength, -1.0f);

  processor.ProcessChunk(in_buffer, &out_buffer);

  EXPECT_THAT(out_buffer.leftCols(processor.latency_frames()),
              EigenArrayNear(
                   Eigen::ArrayXXf::Zero(1, processor.latency_frames()), 1e-5));
  int num_filled_cols = out_buffer.cols() - processor.latency_frames();
  EXPECT_THAT(out_buffer.rightCols(num_filled_cols),
              EigenArrayNear(
                   Eigen::ArrayXXf::Constant(1, num_filled_cols, 2.0f), 1e-5));
  EXPECT_EQ(bypass_callback.block_num(), 8);
}

TEST(SpectralProcessorTest, RampTest) {
  const int kChunkLength = 128;
  const int kBlockLength = 32;
  BypassCallback bypass_callback;
  SpectralProcessorParams params = RectangularWindowHalfOverlap(kBlockLength);

  SpectralProcessor processor(1, 1, kChunkLength, params, &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(1, kChunkLength);
  Eigen::ArrayXXf out_buffer;

  processor.ProcessChunk(in_buffer, &out_buffer);

  EXPECT_THAT(out_buffer.leftCols(processor.latency_frames()),
              EigenArrayNear(
                  Eigen::ArrayXXf::Zero(1, processor.latency_frames()), 1e-5));
  int num_filled_cols = out_buffer.cols() - processor.latency_frames();
  EXPECT_THAT(out_buffer.rightCols(num_filled_cols),
              EigenArrayNear(MakeRamp(1, num_filled_cols), 1e-4));
  EXPECT_EQ(bypass_callback.block_num(), 8);
}

// Make sure this works with uncorrelated channels.
TEST(SpectralProcessorTest, NoiseDifferentInEachChannelTest) {
  const int kChunkLength = 128;
  const int kBlockLength = 32;
  const int kNumChannels = 2;
  BypassCallback bypass_callback;
  SpectralProcessorParams params = RectangularWindowHalfOverlap(kBlockLength);

  SpectralProcessor processor(kNumChannels, kNumChannels, kChunkLength, params,
                              &bypass_callback);

  Eigen::ArrayXXf in_buffer =
      Eigen::ArrayXXf::Random(kNumChannels, kChunkLength);
  Eigen::ArrayXXf out_buffer;

  processor.ProcessChunk(in_buffer, &out_buffer);

  EXPECT_THAT(out_buffer.leftCols(processor.latency_frames()),
              EigenArrayNear(Eigen::ArrayXXf::Zero(kNumChannels,
                                                   processor.latency_frames()),
                             1e-5));
  int num_filled_cols = out_buffer.cols() - processor.latency_frames();
  EXPECT_THAT(out_buffer.rightCols(num_filled_cols),
              EigenArrayNear(in_buffer.leftCols(num_filled_cols), 1e-4));
}

// Make sure this works with uncorrelated channels.
TEST(SpectralProcessorTest, ResetTest) {
  const int kNumChunks = 15;
  const int kNumChannels = 2;
  const int kChunkLength = 32;
  const int kBlockLength = 14;
  const int kFullLengthFrames = kChunkLength * kNumChunks;

  BypassCallback bypass_callback;
  SpectralProcessorParams params(kBlockLength, CosineWindow());

  // Half overlap.
  SpectralProcessor processor(kNumChannels, kNumChannels, kChunkLength, params,
                              &bypass_callback);
  Eigen::ArrayXXf in_buffer =
      Eigen::ArrayXXf::Random(kNumChannels, kFullLengthFrames);
  Eigen::ArrayXXf out_buffer(kNumChannels, kFullLengthFrames);
  Eigen::ArrayXXf out_buffer2(kNumChannels, kFullLengthFrames);

  for (int i = 0; i < kNumChunks; ++i) {
    int offset = kNumChannels * i * kChunkLength;
    processor.ProcessChunk(
        Eigen::Map<const ArrayXXf>(in_buffer.data() + offset, kNumChannels,
                                   kChunkLength),
        Eigen::Map<ArrayXXf>(out_buffer.data() + offset, kNumChannels,
                             kChunkLength));
  }
  processor.Reset();
  for (int i = 0; i < kNumChunks; ++i) {
    int offset = kNumChannels * i * kChunkLength;
    processor.ProcessChunk(
        Eigen::Map<const ArrayXXf>(in_buffer.data() + offset, kNumChannels,
                                   kChunkLength),
        Eigen::Map<ArrayXXf>(out_buffer2.data() + offset, kNumChannels,
                             kChunkLength));
  }

  EXPECT_THAT(out_buffer2, EigenArrayNear(out_buffer, 1e-5));
}

TEST(SpectralProcessorTest, DifferentChunkSizesProduceSameSamplesTest) {
  // The only reason we should expect different chunk sizes to produce the
  // same number of samples is because we are using the BypassCallback.
  const int kChunkLength = 128;
  const int kBlockLength = 32;
  BypassCallback bypass_callback;
  SpectralProcessorParams params = RectangularWindowHalfOverlap(kBlockLength);

  SpectralProcessor processor1(1, 1, kChunkLength, params, &bypass_callback);
  SpectralProcessor processor2(1, 1, kChunkLength / 2, params,
                               &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(1, kChunkLength);
  Eigen::ArrayXXf out_buffer1(1, kChunkLength);
  Eigen::ArrayXXf out_buffer2(1, kChunkLength);

  processor1.ProcessChunk(in_buffer, &out_buffer1);
  processor2.ProcessChunk(
      Eigen::Map<const ArrayXXf>(in_buffer.data(), 1, kChunkLength / 2),
      Eigen::Map<ArrayXXf>(out_buffer2.data(), 1, kChunkLength / 2));
  processor2.ProcessChunk(
      Eigen::Map<const ArrayXXf>(in_buffer.data() + kChunkLength / 2, 1,
                                 kChunkLength / 2),
      Eigen::Map<ArrayXXf>(out_buffer2.data() + kChunkLength / 2, 1,
                           kChunkLength / 2));

  EXPECT_THAT(out_buffer2, EigenArrayNear(out_buffer1, 1e-5));
}

TEST(SpectralProcessorTest, DifferentBlockSizesProduceSameSamplesTest) {
  // The only reason we should expect different block sizes to produce the
  // same samples is because we are using the BypassCallback.
  const int kChunkLength = 128;
  const int kBlockLengthSmall = 16;
  const int kBlockLengthBig = 64;
  BypassCallback bypass_callback;

  // Half overlap.
  SpectralProcessor processor1(1, 1, kChunkLength,
                               RectangularWindowHalfOverlap(kBlockLengthSmall),
                               &bypass_callback);
  SpectralProcessor processor2(1, 1, kChunkLength,
                               RectangularWindowHalfOverlap(kBlockLengthBig),
                               &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(1, kChunkLength);
  Eigen::ArrayXXf out_buffer1(1, kChunkLength);
  Eigen::ArrayXXf out_buffer2(1, kChunkLength);

  processor1.ProcessChunk(in_buffer, &out_buffer1);
  processor2.ProcessChunk(in_buffer, &out_buffer2);

  // They may have different delays, but should otherwise be the same.
  int first_sample = processor1.latency_frames();
  int second_sample = processor2.latency_frames();
  int max_sample = std::max(first_sample, second_sample);
  int num_filled_cols = kChunkLength - max_sample;
  EXPECT_THAT(
      out_buffer1.middleCols(first_sample, num_filled_cols),
      EigenArrayNear(out_buffer2.middleCols(second_sample, num_filled_cols),
                     1e-4));
}

TEST(SpectralProcessorTest, DifferentZeroPaddingProduceSameSamplesTest) {
  // The only reason we should expect different block sizes to produce the
  // same samples is because we are using the BypassCallback.
  const int kChunkLength = 128;
  const int kBlockLength = 16;
  const int kAddedZeroPadding = 64;
  BypassCallback bypass_callback;
  SpectralProcessorParams params = RectangularWindowHalfOverlap(kBlockLength);

  // Half overlap.
  SpectralProcessor processor1(1, 1, kChunkLength, params, &bypass_callback);
  params.fft_length = kAddedZeroPadding;
  SpectralProcessor processor2(1, 1, kChunkLength, params, &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(1, kChunkLength);
  Eigen::ArrayXXf out_buffer1(1, kChunkLength);
  Eigen::ArrayXXf out_buffer2(1, kChunkLength);

  processor1.ProcessChunk(in_buffer, &out_buffer1);
  processor2.ProcessChunk(in_buffer, &out_buffer2);

  // Zero padding does not impact latency.
  EXPECT_EQ(processor1.latency_frames(), processor2.latency_frames());
  int latency = processor1.latency_frames();
  int num_filled_cols = kChunkLength - latency;
  EXPECT_THAT(
      out_buffer1.middleCols(latency, num_filled_cols),
      EigenArrayNear(out_buffer2.middleCols(latency, num_filled_cols), 1e-4));
}

TEST(SpectralProcessorTest, TwentyFivePercentOverlapRampTest) {
  const int kChunkLength = 64;
  const int kBlockLength = 4;
  BypassCallback bypass_callback;

  SpectralProcessorParams params =
      RectangularWindowQuarterOverlap(kBlockLength);

  SpectralProcessor processor(1, 1, kChunkLength, params, &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(1, kChunkLength);
  Eigen::ArrayXXf out_buffer(1, kChunkLength);

  processor.ProcessChunk(in_buffer, &out_buffer);

  EXPECT_THAT(out_buffer.leftCols(processor.latency_frames()),
              EigenArrayNear(
                  Eigen::ArrayXXf::Zero(1, processor.latency_frames()), 1e-5));
  int num_filled_cols = out_buffer.cols() - processor.latency_frames();
  EXPECT_THAT(out_buffer.rightCols(num_filled_cols),
              EigenArrayNear(MakeRamp(1, num_filled_cols), 1e-4));
  EXPECT_EQ(bypass_callback.block_num(), kChunkLength);
}

TEST(SpectralProcessorTest, CheckFrequencyBinTest) {
  const int kChunkLength = 512;
  const int kBlockLength = 128;
  FftCheckerCallback fft_checker;

  // Rectangular window.
  SpectralProcessor processor(1, 1, kChunkLength,
                              RectangularWindowNoOverlap(kBlockLength),
                              &fft_checker);
  const float kSampleRate = 100;
  const float kFrequency = 25;
  const float kAmplitude = 0.5f;
  std::vector<float> in_buffer =
      GenerateSine(kChunkLength, kSampleRate, kFrequency, kAmplitude);
  Eigen::ArrayXXf out_buffer(1, kChunkLength);

  processor.ProcessChunk(
      Eigen::Map<const ArrayXXf>(in_buffer.data(), 1, in_buffer.size()),
      Eigen::Map<ArrayXXf>(out_buffer.data(), 1, out_buffer.size()));
  EXPECT_NEAR(fft_checker.max_bin_rads_per_sample(0),
              2 * M_PI * kFrequency / kSampleRate, 0.04f);
  EXPECT_NEAR(fft_checker.max_bin_magnitude(0), kAmplitude * kBlockLength / 2,
              0.02f);
}

TEST(SpectralProcessorTest, CheckFrequencyBinMultiChannelTest) {
  const int kNumInChannels = 2;
  const int kChunkLength = 512;
  const int kBlockLength = 128;
  FftCheckerCallback fft_checker;

  // Rectangular window.
  SpectralProcessor processor(kNumInChannels, 1, kChunkLength,
                              RectangularWindowNoOverlap(kBlockLength),
                              &fft_checker);

  const float kSampleRate = 100;
  const float kFrequency1 = 25;
  const float kAmplitude1 = 1.0f;
  const float kFrequency2 = 12.5;
  const float kAmplitude2 = 12.0f;
  std::vector<float> freq_1 =
      GenerateSine(kChunkLength, kSampleRate, kFrequency1, kAmplitude1);
  std::vector<float> freq_2 =
      GenerateSine(kChunkLength, kSampleRate, kFrequency2, kAmplitude2);
  Eigen::ArrayXXf two_channel_in(kNumInChannels, kChunkLength);

  for (int i = 0; i < kChunkLength; ++i) {
    two_channel_in(0, i) = freq_1[i];
    two_channel_in(1, i) = freq_2[i];
  }
  Eigen::ArrayXXf out_buffer(1, kChunkLength);

  processor.ProcessChunk(two_channel_in, &out_buffer);
  EXPECT_NEAR(fft_checker.max_bin_rads_per_sample(0),
              2 * M_PI * kFrequency1 / kSampleRate, 0.04f);
  EXPECT_NEAR(fft_checker.max_bin_magnitude(0), kAmplitude1 * kBlockLength / 2,
              0.02f);
  EXPECT_NEAR(fft_checker.max_bin_rads_per_sample(1),
              2 * M_PI * kFrequency2 / kSampleRate, 0.04f);
  EXPECT_NEAR(fft_checker.max_bin_magnitude(1), kAmplitude2 * kBlockLength / 2,
              0.02f);
}

TEST(SpectralProcessorTest, LargerBlocksThanChunksTest) {
  const int kChunkLength = 16;
  const int kNumChunks = 20;
  const int kFullLength = kChunkLength * kNumChunks;
  const int kBlockLength = 64;

  BypassCallback bypass_callback;
  // Half overlap.
  SpectralProcessor processor(1, 1, kChunkLength,
                              RectangularWindowHalfOverlap(kBlockLength),
                              &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(1, kFullLength);
  Eigen::ArrayXXf out_buffer(1, kFullLength);

  for (int i = 0; i < kNumChunks; ++i) {
    int offset = i * kChunkLength;
    processor.ProcessChunk(
        Eigen::Map<const ArrayXXf>(in_buffer.data() + offset, 1, kChunkLength),
        Eigen::Map<ArrayXXf>(out_buffer.data() + offset, 1, kChunkLength));
  }

  EXPECT_THAT(out_buffer.leftCols(processor.latency_frames()),
              EigenArrayNear(
                  Eigen::ArrayXXf::Zero(1, processor.latency_frames()), 1e-5));
  int num_filled_cols = out_buffer.cols() - processor.latency_frames();
  EXPECT_THAT(out_buffer.rightCols(num_filled_cols),
              EigenArrayNear(MakeRamp(1, num_filled_cols), 1e-4));
}

TEST(SpectralProcessorTest, LessInputChannelsThanOutputChannelsTest) {
  const int kChunkLength = 16;
  const int kNumChunks = 20;
  const int kFullLengthFrames = kChunkLength * kNumChunks;
  const int kBlockLength = 64;
  const int kNumInputChannels = 2;
  const int kNumOutputChannels = 3;
  BypassCallback bypass_callback;

  // Half overlap.
  SpectralProcessor processor(
      kNumInputChannels, kNumOutputChannels, kChunkLength,
      RectangularWindowHalfOverlap(kBlockLength), &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(kNumInputChannels, kFullLengthFrames);
  Eigen::ArrayXXf out_buffer(kNumOutputChannels, kFullLengthFrames);

  for (int i = 0; i < kNumChunks; ++i) {
    int in_offset = kNumInputChannels * i * kChunkLength;
    int out_offset = kNumOutputChannels * i * kChunkLength;
    processor.ProcessChunk(
        Eigen::Map<const ArrayXXf>(in_buffer.data() + in_offset,
                                   kNumInputChannels, kChunkLength),
        Eigen::Map<ArrayXXf>(out_buffer.data() + out_offset, kNumOutputChannels,
                             kChunkLength));
  }

  // Channels get copied in the callback from input to output.
  EXPECT_THAT(out_buffer.row(1), EigenArrayNear(out_buffer.row(1), 1e-5));
  EXPECT_THAT(out_buffer.row(2), EigenArrayNear(out_buffer.row(0), 1e-5));
}

TEST(SpectralProcessorTest, MoreInputChannelsThanOutputChannelsTest) {
  const int kChunkLength = 16;
  const int kNumChunks = 20;
  const int kFullLengthFrames = kChunkLength * kNumChunks;
  const int kBlockLength = 64;
  const int kNumInputChannels = 3;
  const int kNumOutputChannels = 2;

  BypassCallback bypass_callback;

  // Half overlap.
  SpectralProcessor processor(
      kNumInputChannels, kNumOutputChannels, kChunkLength,
      RectangularWindowHalfOverlap(kBlockLength), &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(kNumInputChannels, kFullLengthFrames);
  Eigen::ArrayXXf out_buffer(kNumOutputChannels, kFullLengthFrames);

  for (int i = 0; i < kNumChunks; ++i) {
    int in_offset = kNumInputChannels * i * kChunkLength;
    int out_offset = kNumOutputChannels * i * kChunkLength;
    processor.ProcessChunk(
        Eigen::Map<const ArrayXXf>(in_buffer.data() + in_offset,
                                   kNumInputChannels, kChunkLength),
        Eigen::Map<ArrayXXf>(out_buffer.data() + out_offset, kNumOutputChannels,
                             kChunkLength));
  }

  // Channels get copied in the callback from input to output.
  EXPECT_THAT(out_buffer.leftCols(processor.latency_frames()),
              EigenArrayNear(Eigen::ArrayXXf::Zero(kNumOutputChannels,
                                                   processor.latency_frames()),
                             1e-5));
  int num_filled_cols = out_buffer.cols() - processor.latency_frames();
  EXPECT_THAT(
      out_buffer.rightCols(num_filled_cols),
      EigenArrayNear(MakeRamp(kNumOutputChannels, num_filled_cols), 1e-4));
}

TEST(SpectralProcessorTest, CosineWindowPerfectReconstructionTest) {
  const int kChunkLength = 16;
  const int kNumChunks = 20;
  const int kFullLengthFrames = kChunkLength * kNumChunks;
  const int kBlockLength = 64;
  const int kNumInputChannels = 2;
  const int kNumOutputChannels = 2;

  BypassCallback bypass_callback;

  // Cosine window has the perfect reconstruction property with 50% overlap.
  // (the window is applied twice, once before and once after the FFT).
  SpectralProcessorParams params(kBlockLength, CosineWindow());

  // Half overlap.
  SpectralProcessor processor(kNumInputChannels, kNumOutputChannels,
                              kChunkLength, params, &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(kNumInputChannels, kFullLengthFrames);
  Eigen::ArrayXXf out_buffer(kNumOutputChannels, kFullLengthFrames);

  for (int i = 0; i < kNumChunks; ++i) {
    int in_offset = kNumInputChannels * i * kChunkLength;
    int out_offset = kNumOutputChannels * i * kChunkLength;
    processor.ProcessChunk(
        Eigen::Map<const ArrayXXf>(in_buffer.data() + in_offset,
                                   kNumInputChannels, kChunkLength),
        Eigen::Map<ArrayXXf>(out_buffer.data() + out_offset, kNumOutputChannels,
                             kChunkLength));
  }

  // Channels get copied in the callback from input to output.
  EXPECT_THAT(out_buffer.leftCols(processor.latency_frames()),
              EigenArrayNear(Eigen::ArrayXXf::Zero(kNumOutputChannels,
                                                   processor.latency_frames()),
                             1e-5));
  int num_filled_cols = out_buffer.cols() - processor.latency_frames();
  EXPECT_THAT(
      out_buffer.rightCols(num_filled_cols),
      EigenArrayNear(MakeRamp(kNumOutputChannels, num_filled_cols), 1e-4));
}

TEST(SpectralProcessorTest, HammingWindowNoPerfectReconstructionTest) {
  const int kChunkLength = 16;
  const int kNumChunks = 20;
  const int kFullLengthFrames = kChunkLength * kNumChunks;
  const int kBlockLength = 64;
  const int kNumInputChannels = 2;
  const int kNumOutputChannels = 2;

  BypassCallback bypass_callback;

  // Hamming window does not have the perfect reconstruction property
  // (the window is applied twice, once before and once after the FFT).
  SpectralProcessorParams params(kBlockLength, HammingWindow());

  // Half overlap.
  SpectralProcessor processor(kNumInputChannels, kNumOutputChannels,
                              kChunkLength, params, &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(kNumInputChannels, kFullLengthFrames);
  Eigen::ArrayXXf out_buffer(kNumOutputChannels, kFullLengthFrames);

  for (int i = 0; i < kNumChunks; ++i) {
    int in_offset = kNumInputChannels * i * kChunkLength;
    int out_offset = kNumOutputChannels * i * kChunkLength;
    processor.ProcessChunk(
        Eigen::Map<const ArrayXXf>(in_buffer.data() + in_offset,
                                   kNumInputChannels, kChunkLength),
        Eigen::Map<ArrayXXf>(out_buffer.data() + out_offset, kNumOutputChannels,
                             kChunkLength));
  }

  // Channels get copied in the callback from input to output.
  int num_filled_cols = out_buffer.cols() - processor.latency_frames();
  EXPECT_THAT(out_buffer.rightCols(num_filled_cols),
              ::testing::Not(EigenArrayNear(
                  MakeRamp(kNumOutputChannels, num_filled_cols), 1e-4)));
}

TEST(SpectralProcessorTest, NonPowerOfTwoFFTTest) {
  const int kChunkLength = 128;
  const int kBlockLength = 15;
  BypassCallback bypass_callback;

  SpectralProcessor processor(1, 1, kChunkLength,
                              RectangularWindowNoOverlap(kBlockLength),
                              &bypass_callback);
  Eigen::ArrayXXf in_buffer = MakeRamp(1, kChunkLength);
  Eigen::ArrayXXf out_buffer(1, kChunkLength);

  processor.ProcessChunk(in_buffer, &out_buffer);

  for (int i = 0; i < kChunkLength; ++i) {
    float expected = (i < processor.latency_frames())
                         ? 0.0f
                         : i - processor.latency_frames();
    ASSERT_NEAR(out_buffer(0, i), expected, 1e-4f);
  }
  EXPECT_THAT(out_buffer.leftCols(processor.latency_frames()),
              EigenArrayNear(
                  Eigen::ArrayXXf::Zero(1, processor.latency_frames()), 1e-5));
  int num_filled_cols = out_buffer.cols() - processor.latency_frames();
  EXPECT_THAT(out_buffer.rightCols(num_filled_cols),
              EigenArrayNear(MakeRamp(1, num_filled_cols), 1e-4));
}

TEST(SpectralProcessorTest, StreamingTest) {
  const int kNumChunks = 500;
  const int kNumChannels = 2;

  for (int chunk_length : {4, 16, 21, 64, 199, 256}) {
    // Only even block sizes can have perfect reconstruction with 50% overlap.
    for (int block_length : {6, 16, 28, 64, 202, 256}) {
      const int kFullLengthFrames = chunk_length * kNumChunks;
      SCOPED_TRACE("chunk_length: " + testing::PrintToString(chunk_length) +
                   " block_length: " + testing::PrintToString(block_length));

      BypassCallback bypass_callback;

      // Cosine window has the perfect reconstruction property with 50% overlap.
      // (the window is applied twice, once before and once after the FFT).
      SpectralProcessorParams params(block_length, CosineWindow());

      // Half overlap.
      SpectralProcessor processor(kNumChannels, kNumChannels, chunk_length,
                                  params, &bypass_callback);
      Eigen::ArrayXXf in_buffer =
          Eigen::ArrayXXf::Random(kNumChannels, kFullLengthFrames);
      Eigen::ArrayXXf out_buffer(kNumChannels, kFullLengthFrames);

      for (int i = 0; i < kNumChunks; ++i) {
        int offset = kNumChannels * i * chunk_length;
        processor.ProcessChunk(
            Eigen::Map<const ArrayXXf>(in_buffer.data() + offset, kNumChannels,
                                       chunk_length),
            Eigen::Map<ArrayXXf>(out_buffer.data() + offset, kNumChannels,
                                 chunk_length));
      }
      EXPECT_THAT(out_buffer.leftCols(processor.latency_frames()),
                  EigenArrayNear(Eigen::ArrayXXf::Zero(
                                     kNumChannels, processor.latency_frames()),
                                 1e-5));
      int num_filled_cols = out_buffer.cols() - processor.latency_frames();
      EXPECT_THAT(out_buffer.rightCols(num_filled_cols),
                  EigenArrayNear(in_buffer.leftCols(num_filled_cols), 1e-4));
    }
  }
}

TEST(SpectralProcessorTest, StreamingRandomSizesTest) {
  const int kNumChunks = 500;
  const int kNumChannels = 2;
  std::mt19937 rng(0 /* seed */);

  for (int max_chunk_length : {4, 16, 21, 64, 199, 256}) {
    // Only even block sizes can have perfect reconstruction with 50% overlap.
    for (int block_length : {6, 16, 28, 64, 202, 256}) {
      const int kFullLengthFrames = max_chunk_length * kNumChunks;

      BypassCallback bypass_callback;

      // Cosine window has the perfect reconstruction property with 50% overlap.
      // (the window is applied twice, once before and once after the FFT).
      SpectralProcessorParams params(block_length, CosineWindow());

      // Prepare for all possible block sizes.
      std::vector<int> possible_sizes(max_chunk_length);
      for (int i = 0; i < max_chunk_length; ++i) {
        possible_sizes[i] = i + 1;
      }

      SpectralProcessor processor(kNumChannels, kNumChannels, possible_sizes,
                                  params, &bypass_callback);
      Eigen::ArrayXXf in_buffer =
          Eigen::ArrayXXf::Random(kNumChannels, kFullLengthFrames);
      Eigen::ArrayXXf out_buffer(kNumChannels, kFullLengthFrames);

      int offset = 0;
      for (int i = 0; i < kFullLengthFrames;) {
        int upper_bound = std::min(kFullLengthFrames - i, max_chunk_length);
        auto chunk_size_generator =
            std::uniform_int_distribution<>(1, upper_bound);
        const int chunk_size = chunk_size_generator(rng);
        SCOPED_TRACE(
            "max_chunk_length: " + testing::PrintToString(max_chunk_length) +
            "offset: " + testing::PrintToString(offset) +
            "kFullLengthFrames: " + testing::PrintToString(kFullLengthFrames) +
            "chunk_size: " + testing::PrintToString(chunk_size) +
            " block_length: " + testing::PrintToString(block_length));
        processor.ProcessChunk(
            Eigen::Map<const ArrayXXf>(in_buffer.data() + offset, kNumChannels,
                                       chunk_size),
            Eigen::Map<ArrayXXf>(out_buffer.data() + offset, kNumChannels,
                                 chunk_size));
        offset += kNumChannels * chunk_size;
        i += chunk_size;
      }
      EXPECT_THAT(out_buffer.leftCols(processor.latency_frames()),
                  EigenArrayNear(Eigen::ArrayXXf::Zero(
                                     kNumChannels, processor.latency_frames()),
                                 1e-5));
      int num_filled_cols = out_buffer.cols() - processor.latency_frames();
      EXPECT_THAT(out_buffer.rightCols(num_filled_cols),
                  EigenArrayNear(in_buffer.leftCols(num_filled_cols), 1e-4));
    }
  }
}

}  // namespace audio_dsp

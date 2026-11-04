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

#include "audio/dsp/loudness/loudness_1771_two_way_compressor.h"

#include <cstdlib>

#include "audio/dsp/decibels.h"
#include "audio/dsp/hifi/dynamic_range_control_functions.h"
#include "audio/dsp/loudness/streaming_loudness_1771.h"
#include "audio/dsp/testing_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

namespace audio_dsp {
namespace {

// Create parameters for Loudness1771TwoWayCompressor with a two-way compressor
// as a single downwards compressor with the same parameters as a
// DynamicRangeControlParams::ReasonableCompressorParams(). The loudness
// filter is configured by default for two channels of audio.
Loudness1771TwoWayCompressorParams ReasonableCompressorParams() {
  Loudness1771TwoWayCompressorParams params;
  params.attack_s = 0.001;
  params.release_s = 0.08f;
  params.two_way_compression_params.expander_region.threshold_db = -40.0f;
  params.two_way_compression_params.upwards_compressor_region.threshold_db =
      -40.0f;
  params.two_way_compression_params.soft_compressor_region.threshold_db =
      -40.0f;
  params.two_way_compression_params.hard_compressor_region.threshold_db =
      -17.0f;
  params.two_way_compression_params.hard_compressor_region.ratio = 4.6f;
  params.two_way_compression_params.hard_compressor_region.knee_width_db = 4.0f;
  params.output_gain_db = 30.0f;
  return params;
}

TEST(Loudness1771TwoWayCompressor, InputGainTest) {
  constexpr int kNumChannels = 2;
  constexpr int kNumSamples = 4;
  constexpr float kSampleRateHz = 48000.0f;
  Eigen::ArrayXXf input(kNumChannels, kNumSamples);

  // clang-format on
  input << 0.1, 0.1, 0.1, 0.2, 0.3, 0.3, 0.3, 0.0;
  // clang-format off

  Eigen::ArrayXXf output(kNumChannels, kNumSamples);
  // Input gain scales output linearly (below threshold).
  Loudness1771TwoWayCompressorParams params;
  params.streaming_loudness_params =
      StreamingLoudnessParams::Stereo1771Params(0.04, kSampleRateHz);
  params.input_gain_db = AmplitudeRatioToDecibels(2);
  Loudness1771TwoWayCompressor compressor;
  ASSERT_TRUE(
      compressor.Init(params, kNumChannels, kNumSamples, kSampleRateHz));
  ASSERT_TRUE(compressor.ProcessBlock(input, &output));
  EXPECT_THAT(output, EigenArrayNear(2 * input, 1e-4));
}

TEST(Loudness1771TwoWayCompressor, OutputGainTest) {
  constexpr int kNumChannels = 2;
  constexpr int kNumSamples = 4;
  constexpr float kSampleRateHz = 48000.0f;
  Eigen::ArrayXXf input(kNumChannels, kNumSamples);

  // clang-format on
  input << 0.1, 0.1, 0.1, 0.2, 0.3, 0.3, 0.3, 0.0;
  // clang-format off

  Eigen::ArrayXXf output(kNumChannels, kNumSamples);
  // Output gain scales output linearly (below threshold).
  Loudness1771TwoWayCompressorParams params;
  params.streaming_loudness_params =
      StreamingLoudnessParams::Stereo1771Params(0.04, kSampleRateHz);
  params.output_gain_db = AmplitudeRatioToDecibels(2);
  Loudness1771TwoWayCompressor compressor;
  ASSERT_TRUE(
      compressor.Init(params, kNumChannels, kNumSamples, kSampleRateHz));
  ASSERT_TRUE(compressor.ProcessBlock(input, &output));
  EXPECT_THAT(output, EigenArrayNear(2 * input, 1e-4));
}

TEST(Loudness1771TwoWayCompressor, ZeroTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 400;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Zero(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output(kNumChannels, kNumSamples);

  Loudness1771TwoWayCompressorParams params;
  params.streaming_loudness_params.channel_weights = {1.0f};
  Loudness1771TwoWayCompressor compressor;
  ASSERT_TRUE(compressor.Init(params, kNumChannels, kNumSamples, 48000.0f));
  // Make sure that the data is finite;
  ASSERT_TRUE(compressor.ProcessBlock(input, &output));
  ASSERT_TRUE(output.allFinite());
}

TEST(Loudness1771TwoWayCompressor, ResetTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 400;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output1(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output2(kNumChannels, kNumSamples);

  Loudness1771TwoWayCompressorParams params;
  params.streaming_loudness_params.channel_weights = {1.0f};
  Loudness1771TwoWayCompressor compressor;
  ASSERT_TRUE(compressor.Init(params, kNumChannels, kNumSamples, 48000.0f));
  ASSERT_TRUE(compressor.ProcessBlock(input, &output1));
  compressor.Reset();
  ASSERT_TRUE(compressor.ProcessBlock(input, &output2));

  EXPECT_THAT(output1, EigenArrayEq(output2));
}

TEST(Loudness1771TwoWayCompressor, InPlaceTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 400;
  constexpr float kSampleRateHz = 48000.0;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output(kNumChannels, kNumSamples);

  Loudness1771TwoWayCompressorParams params = ReasonableCompressorParams();
  params.streaming_loudness_params.channel_weights = {1.0f};

  Loudness1771TwoWayCompressor compressor;
  ASSERT_TRUE(
      compressor.Init(params, kNumChannels, kNumSamples, kSampleRateHz));
  ASSERT_TRUE(compressor.ProcessBlock(input, &output));
  compressor.Reset();
  ASSERT_TRUE(compressor.ProcessBlock(input, &input));

  EXPECT_THAT(output, EigenArrayEq(input));
}

TEST(Loudness1771TwoWayCompressor, BlockSizeTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 400;
  constexpr float kSampleRateHz = 48000.0;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output_1(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output_2(kNumChannels, kNumSamples);

  Loudness1771TwoWayCompressorParams params;
  params.streaming_loudness_params.channel_weights = {1.0f};

  Loudness1771TwoWayCompressor compressor_1;
  ASSERT_TRUE(
      compressor_1.Init(params, kNumChannels, kNumSamples, kSampleRateHz));
  ASSERT_TRUE(compressor_1.ProcessBlock(input, &output_1));

  Loudness1771TwoWayCompressor compressor_2;
  ASSERT_TRUE(
      compressor_2.Init(params, kNumChannels, kNumSamples * 2, kSampleRateHz));
  ASSERT_TRUE(compressor_2.ProcessBlock(input, &output_2));

  EXPECT_THAT(output_2, EigenArrayNear(output_1, 1e-6));
}

TEST(Loudness1771TwoWayCompressor, LookaheadTest) {
  constexpr int kOneChannel = 1;
  constexpr int kSampleRate = 48000.0f;
  constexpr int kBlockSize = 100;

  Loudness1771TwoWayCompressorParams params;
  params.streaming_loudness_params.channel_weights = {1.0f};

  // No compression happens.
  params.two_way_compression_params.expander_region.threshold_db = 100.0f;
  params.two_way_compression_params.upwards_compressor_region.threshold_db =
      100.0f;
  params.two_way_compression_params.soft_compressor_region.threshold_db =
      100.0f;
  params.two_way_compression_params.hard_compressor_region.threshold_db =
      100.0f;

  Loudness1771TwoWayCompressor compressor;
  ASSERT_TRUE(compressor.Init(params, kOneChannel, kBlockSize, kSampleRate));

  constexpr int kDelaySamples = 3;
  params.lookahead_s = kDelaySamples / static_cast<float>(kSampleRate);
  Loudness1771TwoWayCompressor compressor_delayed;
  ASSERT_TRUE(
      compressor_delayed.Init(params, kOneChannel, kBlockSize, kSampleRate));

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kOneChannel, kBlockSize);

  Eigen::ArrayXXf output(kOneChannel, kBlockSize);
  Eigen::ArrayXXf output_delayed(kOneChannel, kBlockSize);

  ASSERT_TRUE(compressor.ProcessBlock(input, &output));
  ASSERT_TRUE(compressor_delayed.ProcessBlock(input, &output_delayed));

  const int size_minus_delay = kBlockSize - kDelaySamples;
  EXPECT_THAT(output_delayed.rightCols(size_minus_delay),
              EigenArrayNear(output.leftCols(size_minus_delay), 1e-5));
  EXPECT_THAT(
      output_delayed.leftCols(kDelaySamples),
      EigenArrayNear(Eigen::ArrayXXf::Zero(kOneChannel, kDelaySamples), 1e-5));
}

TEST(Loudness1771TwoWayCompressor, LookaheadImpulseTest) {
  constexpr int kOneChannel = 1;
  constexpr int kSampleRate = 48000.0f;
  constexpr int kBlockSize = 100;

  Loudness1771TwoWayCompressorParams params = ReasonableCompressorParams();
  params.streaming_loudness_params.channel_weights = {1.0f};

  // Configure a reasonable compressor. No upwards compression happens.
  params.attack_s = 0.05;

  Loudness1771TwoWayCompressor compressor;
  ASSERT_TRUE(compressor.Init(params, kOneChannel, kBlockSize, kSampleRate));

  constexpr int kDelaySamples = 3;
  params.lookahead_s = kDelaySamples / static_cast<float>(kSampleRate);

  Loudness1771TwoWayCompressor compressor_delayed;
  ASSERT_TRUE(
      compressor_delayed.Init(params, kOneChannel, kBlockSize, kSampleRate));

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kOneChannel, kBlockSize);
  // Add a huge impulse to make sure it gets more suppressed in the lookahead
  // version.
  const int kImpulseTime = 92;
  const int kImpulseLength = 3;
  for (int i = 0; i < kImpulseLength; ++i) {
    input(0, kImpulseTime + i) = 1000.0f;
  }
  Eigen::ArrayXXf output(kOneChannel, kBlockSize);
  Eigen::ArrayXXf output_delayed(kOneChannel, kBlockSize);

  compressor.ProcessBlock(input, &output);
  compressor_delayed.ProcessBlock(input, &output_delayed);

  // Initial impulse is suppressed.
  EXPECT_GT(output(0, kImpulseTime),
            output_delayed(0, kImpulseTime + kDelaySamples) * 1.3);
  // Compressor reacts before impulse happens.
  EXPECT_GT(
      std::abs(output(0, kImpulseTime - 1)),
      std::abs(output_delayed(0, kImpulseTime + kDelaySamples - 1)) * 1.3);
  // Total impulse energy is reduced.
  float output_impulse_energy = 0;
  float output_delayed_impulse_energy = 0;
  for (int i = 0; i < kImpulseLength; ++i) {
    output_impulse_energy += output.square()(0, kImpulseTime + i);
    output_delayed_impulse_energy +=
        output_delayed.square()(0, kImpulseTime + kDelaySamples + i);
  }
  EXPECT_GT(output_impulse_energy, output_delayed_impulse_energy * 1.5);
}

}  // namespace
}  // namespace audio_dsp

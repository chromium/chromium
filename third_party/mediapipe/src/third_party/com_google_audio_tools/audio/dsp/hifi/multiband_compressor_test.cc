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

#include "audio/dsp/hifi/multiband_compressor.h"

#include <complex>
#include <cstdlib>
#include <vector>

#include "audio/dsp/hifi/dynamic_range_control.h"
#include "audio/dsp/kiss_fft.h"
#include "audio/dsp/testing_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

namespace audio_dsp {
namespace {

TEST(MultibandCompressorTest, BasicTest) {
  constexpr float kCrossoverFrequency = 10000.0f;
  // Make the crossover rolloff super steep for easy testing.
  MultibandCompressorParams params(2, {kCrossoverFrequency}, 12);
  // Extremely heavy compression.
  auto* first_stage = params.MutableDynamicRangeControlParams(0);
  first_stage->input_gain_db = 0.0f;
  first_stage->output_gain_db = 0.0f;
  first_stage->threshold_db = -50.0f;
  first_stage->ratio = 6.0f;

  // No compression.
  auto* second_stage = params.MutableDynamicRangeControlParams(1);
  second_stage->input_gain_db = 0.0f;
  second_stage->output_gain_db = 0.0f;
  second_stage->threshold_db = 30.0f;
  second_stage->ratio = 6.0f;

  MultibandCompressor compressor(params);

  constexpr int kNumChannels = 1;
  constexpr int kBlockSize = 2048;
  constexpr int kSampleRate = 48000.0f;
  compressor.Init(kNumChannels, kBlockSize, kSampleRate);

  Eigen::ArrayXXf data = Eigen::ArrayXXf::Random(kNumChannels, kBlockSize);
  data *= 5;
  Eigen::ArrayXXf output = Eigen::ArrayXXf::Zero(kNumChannels, kBlockSize);
  compressor.ProcessBlock(data, &output);

  RealFFTTransformer fft_transformer(kBlockSize, false);
  std::vector<std::complex<float>> original_fft(
      fft_transformer.GetTransformedSize());
  std::vector<std::complex<float>> processed_fft(
      fft_transformer.GetTransformedSize());

  fft_transformer.ForwardTransform(data.data(), original_fft.data());
  fft_transformer.ForwardTransform(output.data(), processed_fft.data());

  // Check that the low frequencies got squashed, but the high frequencies
  // remain.
  constexpr int kLowerBin = 8000.0f / kSampleRate * kBlockSize;
  constexpr int kUpperBin = 12000.0f / kSampleRate * kBlockSize;
  float original_sum = 0;
  float processed_sum = 0;
  for (int i = 0; i < kLowerBin; ++i) {
    original_sum += std::abs(original_fft[i]);
    processed_sum += std::abs(processed_fft[i]);
  }
  EXPECT_GT(original_sum, 50 * processed_sum);

  original_sum = 0;
  processed_sum = 0;
  for (int i = kUpperBin; i < original_fft.size(); ++i) {
    original_sum += std::abs(original_fft[i]);
    processed_sum += std::abs(processed_fft[i]);
  }
  EXPECT_NEAR(original_sum / processed_sum, 1.0, 2e-1);
}

TEST(MultibandCompressorTest, ResetTest) {
  MultibandCompressorParams params(4, {100, 1000, 10000});
  MultibandCompressor compressor(params);

  constexpr int kNumChannels = 3;
  constexpr int kBlockSize = 400;
  constexpr int kSampleRate = 48000.0f;
  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kNumChannels, kBlockSize);
  Eigen::ArrayXXf output1(kNumChannels, kBlockSize);
  Eigen::ArrayXXf output2(kNumChannels, kBlockSize);

  compressor.Init(kNumChannels, kBlockSize, kSampleRate);
  compressor.ProcessBlock(input, &output1);
  compressor.Reset();
  compressor.ProcessBlock(input, &output2);

  EXPECT_THAT(output1, EigenArrayNear(output2, 1e-6));
}

TEST(DynamicRangeControl, InterpolatesCoeffsTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 100;
  constexpr int kNumStages = 4;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kNumChannels, kNumSamples);
  Eigen::ArrayXXf before_output(kNumChannels, kNumSamples);
  Eigen::ArrayXXf after_output(kNumChannels, kNumSamples);
  Eigen::ArrayXXf interp_output(kNumChannels, kNumSamples);

  DynamicRangeControlParams before_drc_params =
      DynamicRangeControlParams::ReasonableCompressorParams();
  DynamicRangeControlParams after_drc_params =
      DynamicRangeControlParams::ReasonableLimiterParams();

  MultibandCompressorParams before_mbc_params(kNumStages, {100, 1000, 10000});
  MultibandCompressorParams after_mbc_params(kNumStages, {100, 1000, 10000});
  // Set the second DRC in the bank.
  *before_mbc_params.MutableDynamicRangeControlParams(2) = before_drc_params;
  *after_mbc_params.MutableDynamicRangeControlParams(2) = after_drc_params;

  for (int i = 0; i < kNumStages; ++i) {
    // Remove most statefulness.
    before_mbc_params.MutableDynamicRangeControlParams(i)->attack_s = 1e-9;
    before_mbc_params.MutableDynamicRangeControlParams(i)->release_s = 1e-9;
    after_mbc_params.MutableDynamicRangeControlParams(i)->attack_s = 1e-9;
    after_mbc_params.MutableDynamicRangeControlParams(i)->release_s = 1e-9;
  }

  MultibandCompressor before_drc(before_mbc_params);
  MultibandCompressor after_drc(after_mbc_params);
  MultibandCompressor interp_drc(before_mbc_params);

  before_drc.Init(kNumChannels, kNumSamples, 48000.0f);
  after_drc.Init(kNumChannels, kNumSamples, 48000.0f);
  interp_drc.Init(kNumChannels, kNumSamples, 48000.0f);
  before_drc.ProcessBlock(input, &before_output);
  after_drc.ProcessBlock(input, &after_output);
  interp_drc.ProcessBlock(input, &interp_output);

  // First block should look like "before" samples.
  EXPECT_THAT(interp_output, EigenArrayNear(before_output, 1e-6));

  interp_drc.SetDynamicRangeControlParams(
      2, *after_mbc_params.MutableDynamicRangeControlParams(2));
  // Clear out the state. Coefficient switch happens during this block.
  // The filterbank on the front end is still stateful, so we have to process
  // the block with after_drc to make sure that each dynamic range compressor
  // sees the same signal.
  interp_drc.ProcessBlock(Eigen::ArrayXXf::Zero(kNumChannels, kNumSamples),
                          &interp_output);
  after_drc.ProcessBlock(Eigen::ArrayXXf::Zero(kNumChannels, kNumSamples),
                         &after_output);
  // Process the input block again.
  interp_drc.ProcessBlock(input, &interp_output);
  after_drc.ProcessBlock(input, &after_output);

  // Third block should look like "after" samples. Note looser tolerance.
  EXPECT_THAT(interp_output, EigenArrayNear(after_output, 1e-6));
}

}  // namespace
}  // namespace audio_dsp

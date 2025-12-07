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

#include "audio/dsp/hifi/dynamic_range_control.h"

#include "audio/dsp/decibels.h"
#include "audio/dsp/hifi/dynamic_range_control_functions.h"
#include "audio/dsp/testing_util.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {
namespace {

float CheckReferenceCompressor(float input_level_db, float threshold_db,
                               float ratio, float knee_width_db) {
  const float half_knee = knee_width_db / 2;
  if (input_level_db - threshold_db <= -half_knee) {
    return input_level_db;
  } else if (input_level_db - threshold_db >= half_knee) {
    return threshold_db + (input_level_db - threshold_db) / ratio;
  } else {
    const float knee_end = input_level_db - threshold_db + half_knee;
    return input_level_db +
           ((1 / ratio) - 1) * knee_end * knee_end / (knee_width_db * 2);
  }
}

// Helper functions for testing a single scalar at a time. Verifies that
// a reference implementation is matched.
float OutputLevelCompressor(float input, float threshold, float ratio,
                            float knee) {
  Eigen::ArrayXf input_arr = Eigen::ArrayXf::Constant(1, input);
  Eigen::ArrayXf output_arr = Eigen::ArrayXf::Constant(1, 0);
  ::audio_dsp::OutputLevelCompressor(input_arr, threshold, ratio, knee,
                                     &output_arr);
  EXPECT_NEAR(CheckReferenceCompressor(input, threshold, ratio, knee),
              output_arr.value(), 1e-4f);
  return output_arr.value();
}

float OutputLevelLimiter(float input, float threshold, float knee) {
  Eigen::ArrayXf input_arr(1);
  Eigen::ArrayXf output_arr(1);
  input_arr[0] = input;
  ::audio_dsp::OutputLevelLimiter(input_arr, threshold, knee, &output_arr);
  // A limiter is a compressor with a ratio of infinity.
  EXPECT_NEAR(
      CheckReferenceCompressor(input, threshold,
                               std::numeric_limits<float>::infinity(), knee),
      output_arr.value(), 1e-4f);
  return output_arr.value();
}

float OutputLevelNoiseGate(float input, float threshold, float knee) {
  Eigen::ArrayXf input_arr(1);
  Eigen::ArrayXf output_arr(1);
  input_arr[0] = input;
  constexpr float kRatio = 1000.0f;
  ::audio_dsp::OutputLevelExpander(input_arr, threshold, kRatio, knee,
                                   &output_arr);
  return output_arr.value();
}

TEST(DynamicRangeControl, InputOutputGainTest) {
  constexpr int kNumChannels = 2;
  constexpr int kNumSamples = 4;
  Eigen::ArrayXXf input(kNumChannels, kNumSamples);

  // clang-format on
  input << 0.1, 0.1, 0.1, 0.2,
          0.3, 0.3, 0.3, 0.0;
  // clang-format off

  Eigen::ArrayXXf output(kNumChannels, kNumSamples);
  {  // Input gain scales output linearly (below threshold).
    DynamicRangeControlParams params;
    params.threshold_db = 200;
    params.input_gain_db = AmplitudeRatioToDecibels(2);
    DynamicRangeControl drc(params);
    drc.Init(kNumChannels, kNumSamples, 48000.0f);
    drc.ProcessBlock(input, &output);
    EXPECT_THAT(output, EigenArrayNear(2 * input, 1e-4));
  }
  {  // Output gain scales output linearly (below threshold).
    DynamicRangeControlParams params;
    params.threshold_db = 200;
    params.output_gain_db = AmplitudeRatioToDecibels(2);
    DynamicRangeControl drc(params);
    drc.Init(kNumChannels, kNumSamples, 48000.0f);
    drc.ProcessBlock(input, &output);
    EXPECT_THAT(output, EigenArrayNear(2 * input, 1e-4));
  }
}

TEST(DynamicRangeControl, LimiterTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 6;
  Eigen::ArrayXXf input(kNumChannels, kNumSamples);
  input << 1.0f, 0.9f, 0.4f, -0.4f, -0.9f, -1.0f;

  Eigen::ArrayXXf output(kNumChannels, kNumSamples);
  {
    DynamicRangeControlParams params =
        DynamicRangeControlParams::ReasonableLimiterParams();
    // We aren't testing the envelope interpolation here. Make time constants
    // small enough that filter behavior is negligable.
    params.attack_s = 1e-6;
    params.release_s = 1e-6;

    params.dynamics_type = kLimiter;
    params.threshold_db = -6.0;
    DynamicRangeControl drc(params);
    drc.Init(kNumChannels, kNumSamples, 48000.0f);
    drc.ProcessBlock(input, &output);

    Eigen::ArrayXXf expected(kNumChannels, kNumSamples);
    float half = DecibelsToAmplitudeRatio(-6.0f);
    expected << half, half, 0.4f, -0.4f, -half, -half;
    EXPECT_THAT(output, EigenArrayNear(expected, 1e-2));
  }
}

TEST(DynamicRangeControl, NoiseGateTest) {
  constexpr int kNumChannels = 2;
  constexpr int kNumSamples = 4;
  Eigen::ArrayXXf input(kNumChannels, kNumSamples);

  // clang-format off
  input << 0.1, 0.1, 0.1, 0.2,
          0.3, 0.3, 0.3, 0.0;
  // clang-format on

  Eigen::ArrayXXf output(kNumChannels, kNumSamples);
  {  // Signal is completely attenuated because it is below the threshold.
    DynamicRangeControlParams params;
    params.dynamics_type = kNoiseGate;
    params.threshold_db = 200;
    DynamicRangeControl drc(params);
    drc.Init(kNumChannels, kNumSamples, 48000.0f);
    drc.ProcessBlock(input, &output);
    EXPECT_THAT(output, EigenArrayNear(0 * input, 1e-4));
  }
}

TEST(DynamicRangeControl, SidechainTest) {
  constexpr int kNumChannels = 2;
  constexpr int kNumSamples = 4;
  Eigen::ArrayXXf input(kNumChannels, kNumSamples);

  // clang-format off
  input << 0.1, 0.1, 0.1, 0.2,
          0.3, 0.3, 0.3, 0.0;
  // clang-format on

  // Since we're using a noise gate, sidechain is basically a time-varying
  // mask.
  Eigen::ArrayXXf sidechain(kNumChannels, kNumSamples);

  // clang-format off
  sidechain << 0.0, 1.0, 0.0, 0.0,
              0.0, 1.0, 1.0, 0.0;
  // clang-format on

  Eigen::ArrayXXf expected(kNumChannels, kNumSamples);

  // clang-format off
  expected << 0.0, 0.1, 0.1, 0.0,
             0.0, 0.3, 0.3, 0.0;
  // clang-format on

  Eigen::ArrayXXf output(kNumChannels, kNumSamples);
  {  // Signal is completely attenuated because it is below the threshold.
    DynamicRangeControlParams params;
    params.dynamics_type = kNoiseGate;
    params.threshold_db = -6.0;
    params.attack_s = 1e-8;
    params.release_s = 1e-8;
    DynamicRangeControl drc(params);
    drc.Init(kNumChannels, kNumSamples, 48000.0f);
    drc.ProcessBlockWithSidechain(input, sidechain, &output);
    EXPECT_THAT(output, EigenArrayNear(expected, 1e-4));
  }
}

// Tests that the gain gets applied.
TEST(DynamicRangeControl, CompressesSignalTest) {
  constexpr int kNumChannels = 2;
  constexpr int kNumSamples = 4;
  constexpr float kInputGainDb = 10;
  Eigen::ArrayXXf input = Eigen::ArrayXXf::Constant(
      kNumChannels, kNumSamples, DecibelsToAmplitudeRatio(kInputGainDb));
  Eigen::ArrayXXf output(kNumChannels, kNumSamples);
  {  // Input is smaller than output because it is above threshold.
    DynamicRangeControlParams params;
    params.envelope_type = kPeak;
    params.threshold_db = 0;
    params.ratio = 5;
    params.attack_s = 1e-8;  // Effectively removes the smoother.
    DynamicRangeControl drc(params);
    drc.Init(kNumChannels, kNumSamples, 48000.0f);
    drc.ProcessBlock(input, &output);

    float expected_db = kInputGainDb / params.ratio;
    EXPECT_FLOAT_EQ(OutputLevelCompressor(kInputGainDb, params.threshold_db,
                                          params.ratio, params.knee_width_db),
                    expected_db);
    Eigen::ArrayXXf expected = Eigen::ArrayXXf::Constant(
        kNumChannels, kNumSamples, DecibelsToAmplitudeRatio(expected_db));
    EXPECT_THAT(output, EigenArrayNear(expected, 1e-4));
  }
  {  // Input is smaller than output because it is well above threshold.
    DynamicRangeControlParams params;
    params.envelope_type = kPeak;
    params.threshold_db = 0;
    params.ratio = 4;
    params.attack_s = 1e-8;  // Effectively removes the smoother.
    DynamicRangeControl drc(params);
    drc.Init(kNumChannels, kNumSamples, 48000.0f);
    drc.ProcessBlock(input, &output);

    float expected_db = kInputGainDb / params.ratio;
    EXPECT_FLOAT_EQ(OutputLevelCompressor(kInputGainDb, params.threshold_db,
                                          params.ratio, params.knee_width_db),
                    expected_db);
    Eigen::ArrayXXf expected = Eigen::ArrayXXf::Constant(
        kNumChannels, kNumSamples, DecibelsToAmplitudeRatio(expected_db));
    EXPECT_THAT(output, EigenArrayNear(expected, 1e-4));
  }
}

TEST(DynamicRangeControl, ZeroTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 400;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Zero(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output(kNumChannels, kNumSamples);

  DynamicRangeControlParams params;
  DynamicRangeControl drc(params);
  drc.Init(kNumChannels, kNumSamples, 48000.0f);
  // Make sure we pass the check that data is finite.
  drc.ProcessBlock(input, &output);
}

TEST(DynamicRangeControl, ResetTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 400;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output1(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output2(kNumChannels, kNumSamples);

  DynamicRangeControlParams params;
  DynamicRangeControl drc(params);
  drc.Init(kNumChannels, kNumSamples, 48000.0f);
  drc.ProcessBlock(input, &output1);
  drc.Reset();
  drc.ProcessBlock(input, &output2);

  EXPECT_THAT(output1, EigenArrayNear(output2, 1e-6));
}

TEST(DynamicRangeControl, ResetChangedParamsTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 400;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output1(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output2(kNumChannels, kNumSamples);

  DynamicRangeControlParams params;
  DynamicRangeControlParams params2;
  params2.input_gain_db = 10;

  DynamicRangeControl drc(params);  // Params will change to params2.
  DynamicRangeControl drc2(params2);
  drc.Init(kNumChannels, kNumSamples, 48000.0f);
  drc2.Init(kNumChannels, kNumSamples, 48000.0f);
  drc.SetDynamicRangeControlParams(params2);
  // Reset should set the params equal to params2 without interpolation.
  drc.Reset();
  drc.ProcessBlock(input, &output1);
  drc2.ProcessBlock(input, &output2);

  EXPECT_THAT(output1, EigenArrayNear(output2, 1e-6));
}


TEST(DynamicRangeControl, InterpolatesCoeffsTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 400;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kNumChannels, kNumSamples);
  Eigen::ArrayXXf before_output(kNumChannels, kNumSamples);
  Eigen::ArrayXXf after_output(kNumChannels, kNumSamples);
  Eigen::ArrayXXf interp_output(kNumChannels, kNumSamples);

  DynamicRangeControlParams before_params =
      DynamicRangeControlParams::ReasonableCompressorParams();
  DynamicRangeControlParams after_params =
      DynamicRangeControlParams::ReasonableLimiterParams();
  // We aren't testing the envelope interpolation here. Make time constants
  // small enough that filter behavior is negligable.
  before_params.attack_s = 1e-6;
  after_params.attack_s = 1e-6;
  before_params.release_s = 1e-6;
  after_params.release_s = 1e-6;
  DynamicRangeControl before_drc(before_params);
  DynamicRangeControl after_drc(after_params);

  DynamicRangeControl interp_drc(before_params);

  before_drc.Init(kNumChannels, kNumSamples, 48000.0f);
  after_drc.Init(kNumChannels, kNumSamples, 48000.0f);
  interp_drc.Init(kNumChannels, kNumSamples, 48000.0f);
  before_drc.ProcessBlock(input, &before_output);
  after_drc.ProcessBlock(input, &after_output);
  // First block should look like "before" samples.
  interp_drc.ProcessBlock(input, &interp_output);
  EXPECT_THAT(interp_output, EigenArrayNear(before_output, 1e-6));

  interp_drc.SetDynamicRangeControlParams(after_params);
  // Clear out the state. Coefficient switch happens during this block.
  interp_drc.ProcessBlock(Eigen::ArrayXXf::Zero(kNumChannels, kNumSamples),
                          &interp_output);

  // Process the input block again.
  interp_drc.ProcessBlock(input, &interp_output);
  // Third block should look like "after" samples. Note looser tolerance.
  EXPECT_THAT(interp_output, EigenArrayNear(after_output, 1e-4));
}

TEST(DynamicRangeControl, InPlaceTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 400;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output(kNumChannels, kNumSamples);

  DynamicRangeControlParams params;
  DynamicRangeControl drc(params);
  drc.Init(kNumChannels, kNumSamples, 48000.0f);
  drc.ProcessBlock(input, &output);
  drc.Reset();
  drc.ProcessBlock(input, &input);

  EXPECT_THAT(output, EigenArrayNear(input, 1e-6));
}

TEST(DynamicRangeControl, BlockSizeTest) {
  constexpr int kNumChannels = 1;
  constexpr int kNumSamples = 400;

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output_1(kNumChannels, kNumSamples);
  Eigen::ArrayXXf output_2(kNumChannels, kNumSamples);

  DynamicRangeControlParams params;
  DynamicRangeControl drc(params);
  drc.Init(kNumChannels, kNumSamples, 48000.0f);
  drc.ProcessBlock(input, &output_1);
  drc.Init(kNumChannels, kNumSamples * 2, 48000.0f);
  drc.ProcessBlock(input, &output_2);

  EXPECT_THAT(output_2, EigenArrayNear(output_1, 1e-6));
}

TEST(DynamicRangeControl, LookaheadTest) {
  constexpr int kOneChannel = 1;
  constexpr int kSampleRate = 48000.0f;
  constexpr int kBlockSize = 1000;
  DynamicRangeControlParams params;

  params.threshold_db = 100.0f;  // No compression.
  DynamicRangeControl drc(params);
  drc.Init(kOneChannel, kBlockSize, kSampleRate);

  constexpr int kDelaySamples = 3;
  params.lookahead_s = kDelaySamples / static_cast<float>(kSampleRate);
  DynamicRangeControl drc_delayed(params);
  drc_delayed.Init(kOneChannel, kBlockSize, kSampleRate);

  Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kOneChannel, kBlockSize);

  Eigen::ArrayXXf output(kOneChannel, kBlockSize);
  Eigen::ArrayXXf output_delayed(kOneChannel, kBlockSize);

  drc.ProcessBlock(input, &output);
  drc_delayed.ProcessBlock(input, &output_delayed);

  const int size_minus_delay = kBlockSize - kDelaySamples;
  EXPECT_THAT(output_delayed.rightCols(size_minus_delay),
              EigenArrayNear(output.leftCols(size_minus_delay), 1e-5));
  EXPECT_THAT(
      output_delayed.leftCols(kDelaySamples),
      EigenArrayNear(Eigen::ArrayXXf::Zero(kOneChannel, kDelaySamples), 1e-5));
}

TEST(DynamicRangeControl, LookaheadImpulseTest) {
  constexpr int kOneChannel = 1;
  constexpr int kSampleRate = 48000.0f;
  constexpr int kBlockSize = 100;
  DynamicRangeControlParams params =
      DynamicRangeControlParams::ReasonableCompressorParams();

  params.attack_s = 0.05;
  DynamicRangeControl drc(params);
  drc.Init(kOneChannel, kBlockSize, kSampleRate);

  constexpr int kDelaySamples = 3;
  params.lookahead_s = kDelaySamples / static_cast<float>(kSampleRate);
  DynamicRangeControl drc_delayed(params);
  drc_delayed.Init(kOneChannel, kBlockSize, kSampleRate);

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

  drc.ProcessBlock(input, &output);
  drc_delayed.ProcessBlock(input, &output_delayed);

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

void BM_Compressor(benchmark::State& state) {
  constexpr int kNumSamples = 1000;
  Eigen::ArrayXf input = Eigen::ArrayXf::Random(kNumSamples);
  Eigen::ArrayXf output(kNumSamples);
  DynamicRangeControl drc(
      DynamicRangeControlParams::ReasonableCompressorParams());
  drc.Init(2, kNumSamples, 48000.0f);

  while (state.KeepRunning()) {
    drc.ProcessBlock(input, &output);
    benchmark::DoNotOptimize(output);
  }
  state.SetItemsProcessed(kNumSamples * state.iterations());
}
BENCHMARK(BM_Compressor);

}  // namespace
}  // namespace audio_dsp

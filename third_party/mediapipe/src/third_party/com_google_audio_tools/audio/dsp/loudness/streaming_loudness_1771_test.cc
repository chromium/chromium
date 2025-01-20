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

#include "audio/dsp/loudness/streaming_loudness_1771.h"

#include <cmath>

#include "audio/dsp/decibels.h"
#include "audio/dsp/signal_generator.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

namespace audio_dsp {
namespace {

using Eigen::ArrayXf;
using Eigen::ArrayXXf;
using Eigen::Map;

// The RMS value of a sine wave peaking at 0 dBFS (decibels full-scale).
constexpr float kRmsScale = 1 / M_SQRT2;
// The number of decimal places of precision required for approximate equality
// when comparing meassured loudness to expected loudness.
constexpr int kPrecision = 1;

// Test that the filter is reset.
TEST(StreamingLoudnessTest, Reset) {
  constexpr int kNumChannels = 1;
  constexpr float kSampleRateHz = 16000;
  constexpr float kSignalLengthSeconds = 1;
  constexpr float kNumSamples = kSampleRateHz * kSignalLengthSeconds;
  constexpr float kTimeConstantS = 0.4;

  ArrayXXf input = ArrayXXf::Random(kNumChannels, kNumSamples);
  ArrayXf output_before_reset = ArrayXf::Zero(kNumSamples);
  ArrayXf output_after_reset = ArrayXf::Zero(kNumSamples);

  StreamingLoudnessParams params =
      StreamingLoudnessParams::Mono1771Params(kTimeConstantS, kSampleRateHz);
  StreamingLoudness1771 loudness;
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));
  loudness.ProcessBlock(input, &output_before_reset);
  loudness.Reset();

  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));
  loudness.ProcessBlock(input, &output_after_reset);

  EXPECT_TRUE(output_before_reset.isApprox(output_after_reset));
}

// This tests isolates the portion of the algorithm that performs RMS on the
// signal from the k-weighting portion of the algorithm on a mono signal.
TEST(StreamingLoudnessTest, RMSMono) {
  constexpr int kNumChannels = 1;
  constexpr float kSampleRateHz = 16000;
  constexpr float kSignalLengthSeconds = 5;
  constexpr int kNumSamples = kSampleRateHz * kSignalLengthSeconds;
  constexpr float kTimeConstantS = 0.4;
  constexpr float kFrequencyHz = 500;
  constexpr float kAmplitude = 0.125;

  const float kExpectedLevel = AmplitudeRatioToDecibels(kAmplitude * kRmsScale);

  // Generate a 500Hz signal with constant amplitude.
  // A 500Hz signal has 0dB gain from the k-weighting filter, so the RMS
  // portion of the ITU-1771 algorithm is isolated.
  ArrayXf one_channel =
      GenerateSineEigen(kNumSamples, kSampleRateHz, kFrequencyHz, kAmplitude);
  Map<ArrayXXf> input_block(one_channel.data(), kNumChannels,
                            one_channel.size());
  ArrayXf output_block = ArrayXf::Zero(kNumSamples);

  StreamingLoudnessParams params =
      StreamingLoudnessParams::Mono1771Params(kTimeConstantS, kSampleRateHz);
  StreamingLoudness1771 loudness;
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));
  loudness.ProcessBlock(input_block, &output_block);

  EXPECT_TRUE(
      output_block.segment(kSampleRateHz, output_block.size() - kSampleRateHz)
          .isApproxToConstant(kExpectedLevel, kPrecision));
}

// This tests isolates the portion of the algorithm that performs RMS on the
// signal from the k-weighting portion of the algorithm on a stereo signal.
TEST(StreamingLoudnessTest, RMSStereo) {
  constexpr int kNumChannels = 2;
  constexpr float kSampleRateHz = 16000;
  constexpr float kSignalLengthSeconds = 5;
  constexpr int kNumSamples = kSampleRateHz * kSignalLengthSeconds;
  constexpr float kTimeConstantS = 0.4;
  constexpr float kFrequencyHz = 500;
  constexpr float kAmplitude = 0.125;

  const float kExpectedLevel = AmplitudeRatioToDecibels(kAmplitude * kRmsScale);

  // Generate a 500Hz signal with constant amplitude.
  // A 500Hz signal has 0dB gain from the k-weighting filter, so the RMS portion
  // of the ITU-1771 algorithm is isolated.
  ArrayXf one_channel =
      GenerateSineEigen(kNumSamples, kSampleRateHz, kFrequencyHz, kAmplitude);
  ArrayXXf input_block = ArrayXXf::Zero(kNumChannels, kNumSamples);
  input_block.row(0) = one_channel;
  input_block.row(1) = one_channel;
  ArrayXf output_block = ArrayXf::Zero(kNumSamples);

  StreamingLoudnessParams params =
      StreamingLoudnessParams::Stereo1771Params(kTimeConstantS, kSampleRateHz);
  StreamingLoudness1771 loudness;
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));
  loudness.ProcessBlock(input_block, &output_block);

  EXPECT_TRUE(
      output_block.segment(kSampleRateHz, output_block.size() - kSampleRateHz)
          .isApproxToConstant(kExpectedLevel, kPrecision));
}

// This tests ProcessVector produces the same result as the above RMSStereo
// test.
TEST(StreamingLoudnessTest, ProcessVectorRMSStereo) {
  constexpr int kNumChannels = 2;
  constexpr float kSampleRateHz = 16000;
  constexpr float kSignalLengthSeconds = 5;
  constexpr int kNumSamples = kSampleRateHz * kSignalLengthSeconds;
  constexpr float kTimeConstantS = 0.4;
  constexpr float kFrequencyHz = 500;
  constexpr float kAmplitude = 0.125;

  const float kExpectedLevel = AmplitudeRatioToDecibels(kAmplitude * kRmsScale);

  // Generate a 500Hz signal with constant amplitude.
  // A 500Hz signal has 0dB gain from the k-weighting filter, so the RMS portion
  // of the ITU-1771 algorithm is isolated.
  ArrayXf one_channel =
      GenerateSineEigen(kNumSamples, kSampleRateHz, kFrequencyHz, kAmplitude);
  ArrayXXf input_block = ArrayXXf::Zero(kNumChannels, kNumSamples);
  input_block.row(0) = one_channel;
  input_block.row(1) = one_channel;
  std::vector<float> input_vector(input_block.size());
  // input_block is column-major channels x frames, so the memory layout is
  // C1 C2 C1 C2 ...
  std::copy_n(input_block.data(), input_block.size(), input_vector.data());
  ArrayXf output_block = ArrayXf::Zero(kNumSamples);

  StreamingLoudnessParams params =
      StreamingLoudnessParams::Stereo1771Params(kTimeConstantS, kSampleRateHz);
  StreamingLoudness1771 loudness;
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));

  std::vector<float> output_vector = loudness.ProcessVector(input_vector);
  Map<ArrayXf> output_map(output_vector.data(), output_vector.size());
  EXPECT_TRUE(
      output_map.segment(kSampleRateHz, output_map.size() - kSampleRateHz)
          .isApproxToConstant(kExpectedLevel, kPrecision));
}

// This tests isolates the portion of the algorithm that performs RMS on the
// signal from the k-weighting portion of the algorithm.
TEST(StreamingLoudnessTest, RMSSurround51Channels) {
  constexpr int kNumChannels = 6;
  constexpr float kSampleRateHz = 16000;
  constexpr float kSignalLengthSeconds = 5;
  constexpr int kNumSamples = kSampleRateHz * kSignalLengthSeconds;
  constexpr float kTimeConstantS = 0.4;
  constexpr float kFrequencyHz = 500;
  constexpr float kAmplitude = 0.125;

  const float kExpectedLevel = AmplitudeRatioToDecibels(kAmplitude * kRmsScale);

  // Generate a 500Hz signal with constant amplitude.
  // A 500Hz signal has 0dB gain from the k-weighting filter, so the RMS portion
  // of the ITU-1771 algorithm is isolated.
  ArrayXf one_channel =
      GenerateSineEigen(kNumSamples, kSampleRateHz, kFrequencyHz, kAmplitude);

  ArrayXXf input_block = ArrayXXf::Zero(kNumChannels, kNumSamples);
  for (int i = 0; i < kNumChannels; ++i) {
    input_block.row(i) = one_channel;
  }

  ArrayXf output_block = ArrayXf::Zero(kNumSamples);

  StreamingLoudnessParams params =
      StreamingLoudnessParams::Surround51Channel1771Params(kTimeConstantS,
                                                           kSampleRateHz);

  StreamingLoudness1771 loudness;
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));
  loudness.ProcessBlock(input_block, &output_block);

  EXPECT_TRUE(
      output_block.segment(kSampleRateHz, output_block.size() - kSampleRateHz)
          .isApproxToConstant(kExpectedLevel, kPrecision));
}

// Test the loudness of pure tones for surround 5.1 channels.
TEST(StreamingLoudnessTest, MomentaryLoudnessPureTones51) {
  constexpr int kNumChannels = 6;
  constexpr float kSampleRateHz = 44100;
  constexpr float kSignalLengthSeconds = 5;
  constexpr int kNumSamples = kSampleRateHz * kSignalLengthSeconds;
  constexpr float kAmplitude = 0.125;
  constexpr float kTimeConstantS = 0.40;
  constexpr float kExpectedLoudness =
      -13.9;  // Calculated using ffmpeg ebur128 filter

  ArrayXXf input_block = ArrayXXf::Zero(kNumChannels, kNumSamples);

  StreamingLoudnessParams params =
      StreamingLoudnessParams::Surround51Channel1771Params(kTimeConstantS,
                                                           kSampleRateHz);

  StreamingLoudness1771 loudness;
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));

  // Each channel is a sine wave of a different frequency.
  // Left channel: 200 Hz.
  input_block.row(0) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 200, kAmplitude);
  // Right channel: 400 Hz.
  input_block.row(1) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 400, kAmplitude);
  // Center channel: 600 Hz.
  input_block.row(2) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 600, kAmplitude);
  // LFE channel: 50 Hz.
  input_block.row(3) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 50, kAmplitude);
  // Left back channel: 800 Hz.
  input_block.row(4) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 800, kAmplitude);
  // Right back channel: 1000 Hz.
  input_block.row(5) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 1000, kAmplitude);

  ArrayXf output_block = ArrayXf::Zero(kNumSamples);

  loudness.ProcessBlock(input_block, &output_block);

  // The loudness measurement should reach steady state by this time.
  int steady_state_index = kSampleRateHz;
  EXPECT_TRUE(
      output_block.segment(steady_state_index, kNumSamples - steady_state_index)
          .isApproxToConstant(kExpectedLoudness, kPrecision));
}

// Test the loudness of pure tones downsampled to stereo.
TEST(StreamingLoudnessTest, MomentaryLoudnessPureTonesStereo) {
  constexpr int kNumChannels = 2;
  constexpr float kSampleRateHz = 44100;
  constexpr float kSignalLengthSeconds = 5;
  constexpr int kNumSamples = kSampleRateHz * kSignalLengthSeconds;
  constexpr float kAmplitude = 0.125;
  constexpr float kTimeConstantS = 0.40;
  constexpr float kExpectedLoudness =
      -17.9;  // Measured with ffmpeg ebur128 filter.

  StreamingLoudnessParams params =
      StreamingLoudnessParams::Stereo1771Params(kTimeConstantS, kSampleRateHz);

  StreamingLoudness1771 loudness;
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));

  // Generate 5 tones that are mixed in a stereo audio signal, to emulate how
  // 5.1 would be downmixed to stereo before loudness measurement. The LFE
  // channel is not used for downmixing, so it is not generated.
  ArrayXf front_left =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 200, kAmplitude);
  ArrayXf front_right =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 400, kAmplitude);
  ArrayXf center =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 600, kAmplitude);
  ArrayXf back_left =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 800, kAmplitude);
  ArrayXf back_right =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 1000, kAmplitude);

  ArrayXXf input_block = ArrayXXf::Zero(kNumChannels, kNumSamples);

  // Stereo left channel.
  input_block.row(0) = 0.3 * front_left + 0.3 * back_left + center;
  // Stereo right channel.
  input_block.row(1) = 0.3 * front_right + 0.3 * back_right + center;

  ArrayXf output_block = ArrayXf::Zero(kNumChannels);

  loudness.ProcessBlock(input_block, &output_block);

  // The loudness measurement should reach steady state by this time.
  int steady_state_index = kSampleRateHz;
  EXPECT_TRUE(
      output_block.segment(steady_state_index, kNumSamples - steady_state_index)
          .isApproxToConstant(kExpectedLoudness, kPrecision));
}

// Test the loudness of pure tones downsampled to mono.
TEST(StreamingLoudnessTest, MomentaryLoudnessPureTonesMono) {
  constexpr int kNumChannels = 1;
  constexpr float kSampleRateHz = 44100;
  constexpr float kSignalLengthSeconds = 5;
  constexpr int kNumSamples = kSampleRateHz * kSignalLengthSeconds;
  constexpr float kAmplitude = 0.125;
  constexpr float kTimeConstantS = 0.40;
  constexpr float kExpectedLoudness =
      -21.3;  // measured with ffmpeg ebur128 filter
  StreamingLoudnessParams params =
      StreamingLoudnessParams::Mono1771Params(kTimeConstantS, kSampleRateHz);

  StreamingLoudness1771 loudness;
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));

  // Generate 5 tones that are mixed in a mono audio signal, to emulate how
  // 5.1 would be downmixed to mono before loudness measurement. The LFE
  // channel is not used for downmixing, so it is not generated.
  ArrayXf front_left =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 200, kAmplitude);
  ArrayXf front_right =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 400, kAmplitude);
  ArrayXf center =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 600, kAmplitude);
  ArrayXf back_left =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 800, kAmplitude);
  ArrayXf back_right =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 1000, kAmplitude);

  ArrayXXf input_block = ArrayXXf::Zero(kNumChannels, kNumSamples);
  input_block.row(0) = 0.3 * front_left + 0.3 * back_left + center +
                       0.3 * front_right + 0.3 * back_right + center;

  ArrayXf output_block = ArrayXf::Zero(kNumSamples);

  loudness.ProcessBlock(input_block, &output_block);

  // The loudness measurement should reach steady state by this time.
  int steady_state_index = kSampleRateHz;
  EXPECT_TRUE(
      output_block.segment(steady_state_index, kNumSamples - steady_state_index)
          .isApproxToConstant(kExpectedLoudness, kPrecision));
}

// Test to make sure resampled versions of the same audio data are close in
// loudness.
TEST(StreamingLoudnessTest, ResampledLoudness) {
  constexpr int kNumChannels = 6;
  constexpr float kSampleRate16kHz = 16000;
  constexpr float kSampleRate441kHz = 44100;
  constexpr float kSignalLengthSeconds = 5;
  constexpr float kNumSamples16kHz = kSampleRate16kHz * kSignalLengthSeconds;
  constexpr float kNumSamples441kHz = kSampleRate441kHz * kSignalLengthSeconds;
  constexpr float kAmplitude = 0.125;
  constexpr float kTimeConstantS = 0.40;

  // 16 kHz audio.
  ArrayXXf input_block_16k = ArrayXXf::Zero(kNumChannels, kNumSamples16kHz);

  StreamingLoudnessParams params_16k =
      StreamingLoudnessParams::Surround51Channel1771Params(kTimeConstantS,
                                                           kSampleRate16kHz);

  StreamingLoudness1771 loudness;
  ASSERT_TRUE(loudness.Init(params_16k, kNumChannels, kSampleRate16kHz));

  // Each channel is a sine wave of a different frequency.
  // Left channel: 200 Hz.
  input_block_16k.row(0) =
      GenerateSineEigen(kNumSamples16kHz, kSampleRate16kHz, 200, kAmplitude);
  // Right channel: 400 Hz.
  input_block_16k.row(1) =
      GenerateSineEigen(kNumSamples16kHz, kSampleRate16kHz, 400, kAmplitude);
  // Center channel: 600 Hz.
  input_block_16k.row(2) =
      GenerateSineEigen(kNumSamples16kHz, kSampleRate16kHz, 600, kAmplitude);
  // LFE channel: 50 Hz.
  input_block_16k.row(3) =
      GenerateSineEigen(kNumSamples16kHz, kSampleRate16kHz, 50, kAmplitude);
  // Left back channel: 800 Hz.
  input_block_16k.row(4) =
      GenerateSineEigen(kNumSamples16kHz, kSampleRate16kHz, 800, kAmplitude);
  // Right back channel: 1000 Hz.
  input_block_16k.row(5) =
      GenerateSineEigen(kNumSamples16kHz, kSampleRate16kHz, 1000, kAmplitude);

  ArrayXf output_block_16k = ArrayXf::Zero(kNumSamples16kHz);

  loudness.ProcessBlock(input_block_16k, &output_block_16k);

  loudness.Reset();

  // The loudness measurement should reach steady state by this time.
  float steady_state_loudness_16k =
      output_block_16k
          .segment(kSampleRate16kHz, output_block_16k.size() - kSampleRate16kHz)
          .mean();

  // 44.1 kHz audio.
  ArrayXXf input_block_441k = ArrayXXf::Zero(kNumChannels, kNumSamples441kHz);

  StreamingLoudnessParams params_441k =
      StreamingLoudnessParams::Surround51Channel1771Params(kTimeConstantS,
                                                           kSampleRate441kHz);
  ASSERT_TRUE(loudness.Init(params_441k, kNumChannels, kSampleRate441kHz));

  // Each channel is a sine wave of a different frequency.
  // Left channel: 200 Hz.
  input_block_441k.row(0) =
      GenerateSineEigen(kNumSamples441kHz, kSampleRate441kHz, 200, kAmplitude);
  // Right channel: 400 Hz.
  input_block_441k.row(1) =
      GenerateSineEigen(kNumSamples441kHz, kSampleRate441kHz, 400, kAmplitude);
  // Center channel: 600 Hz.
  input_block_441k.row(2) =
      GenerateSineEigen(kNumSamples441kHz, kSampleRate441kHz, 600, kAmplitude);
  // LFE channel: 50 Hz.
  input_block_441k.row(3) =
      GenerateSineEigen(kNumSamples441kHz, kSampleRate441kHz, 50, kAmplitude);
  // Left back channel: 800 Hz.
  input_block_441k.row(4) =
      GenerateSineEigen(kNumSamples441kHz, kSampleRate441kHz, 800, kAmplitude);
  // Right back channel: 1000 Hz.
  input_block_441k.row(5) =
      GenerateSineEigen(kNumSamples441kHz, kSampleRate441kHz, 1000, kAmplitude);

  ArrayXf output_block_441k = ArrayXf::Zero(kNumSamples441kHz);

  loudness.ProcessBlock(input_block_441k, &output_block_441k);

  loudness.Reset();

  // The loudness measurement should reach steady state by this time.
  float steady_state_loudness_441k =
      output_block_441k
          .segment(kSampleRate441kHz,
                   output_block_441k.size() - kSampleRate441kHz)
          .mean();

  EXPECT_NEAR(steady_state_loudness_16k, steady_state_loudness_441k, 0.01);
}

// This test isolates single channels in a multichannel signal to verify channel
// weighting works as expected.
TEST(StreamingLoudnessTest, ChannelMasking) {
  constexpr int kNumChannels = 6;
  constexpr float kTimeConstantS = 0.4;
  constexpr float kSampleRateHz = 44100;
  constexpr float kSignalLengthSeconds = 5;
  constexpr int kNumSamples = kSampleRateHz * kSignalLengthSeconds;
  // Each channel is a sine wave of a unique amlitude.
  constexpr float kAmplitudeLeft = 0.25;
  constexpr float kAmplitudeRight = 0.5;
  constexpr float kAmplitudeCenter = 0.125;
  constexpr float kAmplitudeLfe = 0.75;
  constexpr float kAmplitudeLeftBack = 0.03125;
  constexpr float kAmplitudeRightBack = 0.0625;

  ArrayXXf input_block = ArrayXXf::Zero(kNumChannels, kNumSamples);

  // Each channel is a sine wave of a different amplitude.
  // Left channel.
  input_block.row(0) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 200, kAmplitudeLeft);
  // Right channel.
  input_block.row(1) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 400, kAmplitudeRight);
  // Center channel.
  input_block.row(2) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 600, kAmplitudeCenter);
  // LFE channel.
  input_block.row(3) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 50, kAmplitudeLfe);
  // Left back channel.
  input_block.row(4) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 800, kAmplitudeLeftBack);
  // Right back channel.
  input_block.row(5) =
      GenerateSineEigen(kNumSamples, kSampleRateHz, 1000, kAmplitudeRightBack);

  ArrayXf output_block = ArrayXf::Zero(kNumSamples);

  StreamingLoudnessParams params =
      StreamingLoudnessParams::Surround51Channel1771Params(kTimeConstantS,
                                                           kSampleRateHz);

  StreamingLoudness1771 loudness;

  // Mask all but the left front channel.
  params.channel_weights = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));
  loudness.ProcessBlock(input_block, &output_block);
  loudness.Reset();
  EXPECT_TRUE(output_block.segment(kSampleRateHz, kNumSamples - kSampleRateHz)
                  .isApproxToConstant(
                      AmplitudeRatioToDecibels(kRmsScale * kAmplitudeLeft),
                      kPrecision));

  // Mask all but the right front channel.
  params.channel_weights = {0.0, 1.0, 0.0, 0.0, 0.0, 0.0};
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));
  loudness.ProcessBlock(input_block, &output_block);
  loudness.Reset();
  EXPECT_TRUE(output_block.segment(kSampleRateHz, kNumSamples - kSampleRateHz)
                  .isApproxToConstant(
                      AmplitudeRatioToDecibels(kRmsScale * kAmplitudeRight),
                      kPrecision));

  // Mask all but the center channel.
  params.channel_weights = {0.0, 0.0, 1.0, 0.0, 0.0, 0.0};
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));
  loudness.ProcessBlock(input_block, &output_block);
  loudness.Reset();
  EXPECT_TRUE(output_block.segment(kSampleRateHz, kNumSamples - kSampleRateHz)
                  .isApproxToConstant(
                      AmplitudeRatioToDecibels(kRmsScale * kAmplitudeCenter),
                      kPrecision));

  // Mask all but the left back channel.
  params.channel_weights = {0.0, 0.0, 0.0, 0.0, 1.0, 0.0};
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));
  loudness.ProcessBlock(input_block, &output_block);
  loudness.Reset();
  EXPECT_TRUE(output_block.segment(kSampleRateHz, kNumSamples - kSampleRateHz)
                  .isApproxToConstant(
                      AmplitudeRatioToDecibels(kRmsScale * kAmplitudeLeftBack),
                      kPrecision));

  // Mask all but the right back channel.
  params.channel_weights = {0.0, 0.0, 0.0, 0.0, 0.0, 1.0};
  ASSERT_TRUE(loudness.Init(params, kNumChannels, kSampleRateHz));
  loudness.ProcessBlock(input_block, &output_block);
  EXPECT_TRUE(output_block.segment(kSampleRateHz, kNumSamples - kSampleRateHz)
                  .isApproxToConstant(
                      AmplitudeRatioToDecibels(kRmsScale * kAmplitudeRightBack),
                      kPrecision));
}

}  // namespace
}  // namespace audio_dsp

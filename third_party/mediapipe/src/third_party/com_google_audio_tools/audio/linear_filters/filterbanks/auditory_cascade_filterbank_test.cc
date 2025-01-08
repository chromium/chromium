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

#include "audio/linear_filters/filterbanks/auditory_cascade_filterbank.h"

#include "audio/dsp/testing_util.h"
#include "audio/linear_filters/filterbanks/auditory_cascade_filterbank_params.pb.h"
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"
#include "third_party/eigen3/Eigen/Core"

namespace linear_filters {
namespace {

using absl::StrFormat;

class AuditoryFilterbankBuilderTestWithParam
    : public ::testing::TestWithParam<bool> {
 protected:
  static constexpr float kSampleRate = 48000.0f;
  static constexpr int kNumMics = 4;

  AuditoryFilterbankBuilderTestWithParam()
      : cascade_(params_, GetParam()) {
    cascade_.Init(kNumMics, kSampleRate);
  }

  AuditoryCascadeFilterbankParams params_;
  AuditoryCascadeFilterbank cascade_;
};

INSTANTIATE_TEST_SUITE_P(AllowingDownsampling,
                         AuditoryFilterbankBuilderTestWithParam,
                         testing::Values(false, true));

TEST_P(AuditoryFilterbankBuilderTestWithParam, BasicTest) {
  // 75 filters are in the default filterbank. This is consistent with the
  // python test in car_linear_test.py.
  EXPECT_EQ(cascade_.GetFilterbankSize(), 75);

  const BiquadFilterCascadeCoefficients& coeffs =
      cascade_.GetCoefficients();

  // Check that the peak frequencies of the biquad stages are monotonically
  // decreasing and that the number of samples per cycle isn't too high.
  float previous_peak_frequency_hz = kSampleRate;
  for (int stage = 0; stage < cascade_.GetFilterbankSize(); ++stage) {
    SCOPED_TRACE(StrFormat("Stage %d. (decimation=%d)", stage, GetParam()));
    const float stage_sample_rate = cascade_.GetSampleRate(stage);
    const BiquadFilterCoefficients& stage_coeffs = coeffs[stage];
    float peak_frequency_rads_per_sample =
        stage_coeffs.FindPeakFrequencyRadiansPerSample().first;
    float peak_frequency_hz =
        peak_frequency_rads_per_sample / (2 * M_PI) * stage_sample_rate;
    if (GetParam() && stage_sample_rate >= 2 * params_.min_sample_rate()) {
      // The peak frequency is generally a bit higher than the peak frequency.
      // Since the requirement is on the peak and not the pole, we add a bit of
      // tolerance.
      ASSERT_LT(stage_sample_rate / peak_frequency_hz,
                1.05 * params_.max_samples_per_cycle());
    }
    ASSERT_LT(peak_frequency_hz, previous_peak_frequency_hz);
    previous_peak_frequency_hz = peak_frequency_hz;
  }

  // Make sure the downsampling actually happens (only when it is supposed to).
  int num_samples = 512;
  Eigen::ArrayXXf samples = Eigen::ArrayXXf::Random(kNumMics, num_samples);
  cascade_.ProcessBlock(samples);
  for (int stage = 0; stage < cascade_.GetFilterbankSize(); ++stage) {
    // If decimation is allowed, it's not the first stage, and if decimation
    // actually occurred for this stage.
    if (GetParam() && stage > 0 &&
        cascade_.GetSampleRate(stage) < cascade_.GetSampleRate(stage - 1)) {
      num_samples /= 2;
    }
    ASSERT_EQ(cascade_.FilteredOutput(stage).cols(), num_samples);
  }
}

TEST_P(AuditoryFilterbankBuilderTestWithParam, ResetTest) {
  int num_samples = 32;
  Eigen::ArrayXXf samples = Eigen::ArrayXXf::Random(kNumMics, num_samples);
  cascade_.ProcessBlock(samples);
  std::vector<Eigen::ArrayXXf> expected(cascade_.GetFilterbankSize());
  for (int i = 0; i < cascade_.GetFilterbankSize(); ++i) {
    expected[i] = cascade_.FilteredOutput(i);
  }
  cascade_.Reset();
  cascade_.ProcessBlock(samples);
  for (int i = 0; i < cascade_.GetFilterbankSize(); ++i) {
    ASSERT_THAT(cascade_.FilteredOutput(i),
                audio_dsp::EigenArrayNear(expected[i], 1e-5));
  }
}

TEST_P(AuditoryFilterbankBuilderTestWithParam, PeaksAreOneTest) {
  // Computes the gain from all stages and accounts for the differentiator.
  auto FilterbankGainMagnitude = [this](int num_stages, float frequency_hz) {
    BiquadFilterCoefficients differentiator({1, -1, 0}, {1, 0, 0});
    const BiquadFilterCascadeCoefficients& coeffs =
        cascade_.GetCoefficients();
    float gain = differentiator.GainMagnitudeAtFrequency(
        frequency_hz, cascade_.GetSampleRate(num_stages));
    for (int i = 0; i <= num_stages; ++i) {
      gain *= coeffs[i].GainMagnitudeAtFrequency(frequency_hz,
                                                 cascade_.GetSampleRate(i));
    }
    return gain;
  };

  for (int stage = 0; stage < cascade_.GetFilterbankSize(); ++stage) {
    ASSERT_GT(cascade_.BandwidthHz(stage), 24.0);
    ASSERT_LT(cascade_.BandwidthHz(stage), 3000.0);
    float peak_frequency = cascade_.PeakFrequencyHz(stage);
    SCOPED_TRACE(StrFormat("Stage %d with peak = %f. (decimation=%d)", stage,
                           peak_frequency, GetParam()));
    float gain = FilterbankGainMagnitude(stage, peak_frequency);
    ASSERT_NEAR(gain, 1.0, 5e-5);
    // Make sure it actually is a peak.
    ASSERT_GT(gain, FilterbankGainMagnitude(stage, peak_frequency * 1.01));
    ASSERT_GT(gain, FilterbankGainMagnitude(stage, peak_frequency * 0.99));
  }
}

AuditoryCascadeFilterbankParams ChannelSubsetParams(
    int start, int skip, int max) {
  AuditoryCascadeFilterbankParams params;
  params.mutable_channel_selection_options()->set_use_all_channels(false);
  params.mutable_channel_selection_options()->set_first_channel(start);
  params.mutable_channel_selection_options()->set_skip_every_n(skip);
  params.mutable_channel_selection_options()->set_max_channels(max);
  return params;
}


TEST(AuditoryFilterbankBuilderTest, SelectSubsetTest) {
  constexpr float kSampleRate = 48000.0f;
  constexpr int kNumMics = 1;
  // Anything greater than 100 or so is fine.
  constexpr int kUnlimitedChannels = 10000;

  AuditoryCascadeFilterbankParams default_params;
  AuditoryCascadeFilterbank default_filter(default_params);
  default_filter.Init(kNumMics, kSampleRate);
  {  // Drop the first 4 channels.
    AuditoryCascadeFilterbankParams params =
        ChannelSubsetParams(4, 1, kUnlimitedChannels);
    AuditoryCascadeFilterbank filter(params);
    filter.Init(kNumMics, kSampleRate);
    EXPECT_EQ(filter.GetFilterbankSize(),
              default_filter.GetFilterbankSize() - 4);
    EXPECT_EQ(filter.PeakFrequencyHz(0),
              default_filter.PeakFrequencyHz(4));
  }
  {  // Drop every other channel.
    AuditoryCascadeFilterbankParams params =
        ChannelSubsetParams(0, 2, kUnlimitedChannels);
    AuditoryCascadeFilterbank filter(params);
    filter.Init(kNumMics, kSampleRate);
    EXPECT_EQ(filter.GetFilterbankSize(),
              // If there is an odd number of channels, we'll take the last one.
              (default_filter.GetFilterbankSize() + 1) / 2);
    EXPECT_EQ(filter.PeakFrequencyHz(0),
              default_filter.PeakFrequencyHz(0));
    EXPECT_EQ(filter.PeakFrequencyHz(1),
              default_filter.PeakFrequencyHz(2));
    EXPECT_EQ(filter.PeakFrequencyHz(5),
              default_filter.PeakFrequencyHz(10));
    EXPECT_EQ(filter.PeakFrequencyHz(14),
              default_filter.PeakFrequencyHz(28));
  }
  {  // Use at most 30 channels.
    AuditoryCascadeFilterbankParams params =
        ChannelSubsetParams(0, 1, 30);
    AuditoryCascadeFilterbank filter(params);
    filter.Init(kNumMics, kSampleRate);
    EXPECT_EQ(filter.GetFilterbankSize(), 30);
    for (int i = 0; i < filter.GetFilterbankSize(); ++i) {
      ASSERT_EQ(filter.PeakFrequencyHz(i),
                default_filter.PeakFrequencyHz(i));
    }
  }
  {  // Use at most 15 channels, starting at index 3 and taking every 4.
    AuditoryCascadeFilterbankParams params =
        ChannelSubsetParams(3, 4, 15);
    AuditoryCascadeFilterbank filter(params);
    filter.Init(kNumMics, kSampleRate);
    EXPECT_EQ(filter.GetFilterbankSize(), 15);
    for (int i = 0; i < filter.GetFilterbankSize(); ++i) {
      ASSERT_EQ(filter.PeakFrequencyHz(i),
                default_filter.PeakFrequencyHz(4 * i + 3));
    }
  }
  {  // Use at most 15 channels, starting at index 1 and taking every 10.
    AuditoryCascadeFilterbankParams params =
        ChannelSubsetParams(1, 10, 15);
    AuditoryCascadeFilterbank filter(params);
    filter.Init(kNumMics, kSampleRate);
    EXPECT_LE(filter.GetFilterbankSize(), 15);
    // There weren't enough for all 10 channels.
    EXPECT_EQ(filter.GetFilterbankSize(), 8);
    for (int i = 0; i < filter.GetFilterbankSize(); ++i) {
      ASSERT_EQ(filter.PeakFrequencyHz(i),
                default_filter.PeakFrequencyHz(10 * i + 1));
    }
  }
  {  // Test some parameters that would be suitable for a hotword model.
    AuditoryCascadeFilterbankParams params =
        ChannelSubsetParams(0, 2, 40);
    params.set_highest_pole_frequency(6800);
    params.set_step_erbs(0.35);

    AuditoryCascadeFilterbank filter(params);
    filter.Init(kNumMics, 16000);

    EXPECT_EQ(filter.GetFilterbankSize(), 40);
  }
}

}  // namespace
}  // namespace linear_filters

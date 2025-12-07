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

#include "audio/dsp/mfcc/mel_filterbank.h"

#include <cmath>
#include <cstdlib>

#include "audio/dsp/signal_vector_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/random/random.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {
namespace {

using ::testing::Each;
using ::testing::Gt;

TEST(MelFilterbankTest, AgreesWithPythonGoldenValues) {
  // This test verifies the Mel filterbank against "golden values".
  // Golden values are from an independent Python Mel implementation.
  MelFilterbank filterbank;

  std::vector<double> input;
  const int kSampleCount = 513;
  for (int i = 0; i < kSampleCount; ++i) {
    input.push_back(i + 1);
  }
  const int kChannelCount = 20;
  filterbank.Initialize(input.size(),
                        /*sample_rate=*/22050, kChannelCount,
                        /*lower_frequency_limit=*/20.0,
                        /*upper_frequency_limit=*/4000.0);

  std::vector<double> output;
  filterbank.Compute(input, &output);

  std::vector<double> expected = {
      7.38894574,   10.30330648, 13.72703292,  17.24158686,  21.35253118,
      25.77781089,  31.30624108, 37.05877236,  43.9436536,   51.80306637,
      60.79867148,  71.14363376, 82.90910141,  96.50069158,  112.08428368,
      129.96721968, 150.4277597, 173.74997634, 200.86037462, 231.59802942};

  ASSERT_EQ(output.size(), kChannelCount);

  for (int i = 0; i < kChannelCount; ++i) {
    EXPECT_NEAR(output[i], expected[i], 1e-04);
  }
}

TEST(MelFilterbankTest, IgnoresExistingContentOfOutputVector) {
  // Test for bug where the output vector was not cleared before
  // accumulating next frame's weighted spectral values.
  MelFilterbank filterbank;

  const int kSampleCount = 513;
  const int kChannelCount = 20;
  std::vector<double> input;
  std::vector<double> output;

  filterbank.Initialize(kSampleCount,
                        /*sample_rate=*/22050, kChannelCount,
                        /*lower_frequency_limit=*/20.0,
                        /*upper_frequency_limit=*/4000.0);

  // First call with nonzero input value, and an empty output vector,
  // will resize the output and fill it with the correct, nonzero outputs.
  input.assign(kSampleCount, 1.0);
  filterbank.Compute(input, &output);
  EXPECT_THAT(output, Each(Gt(0.0)));

  // Second call with zero input should also generate zero output.  However,
  // the output vector now is already the correct size, but full of nonzero
  // values.  Make sure these don't affect the output.
  input.assign(kSampleCount, 0.0);
  filterbank.Compute(input, &output);
  EXPECT_THAT(output, Each(0.0));

  // Perform similar test for Invert(). First call from above not tested since
  // a non-zero input does not result in a non-zero value for all bins, as DC is
  // excluded.
  input.assign(kChannelCount, 0.0);
  output.assign(kSampleCount, 1.0);
  filterbank.EstimateInverse(input, &output);
  EXPECT_THAT(output, Each(0.0));
}

TEST(MelFilterbankTest, LowerEdgeAtZeroIsOk) {
  // Original code objected to lower_frequency_edge == 0, but it's OK really.
  MelFilterbank filterbank;

  std::vector<double> input;
  const int kSampleCount = 513;
  for (int i = 0; i < kSampleCount; ++i) {
    input.push_back(i + 1);
  }
  const int kChannelCount = 20;
  filterbank.Initialize(input.size(),
                        /*sample_rate=*/22050, kChannelCount,
                        /*lower_frequency_limit=*/0.0,
                        /*upper_frequency_limit=*/4000.0);

  std::vector<double> output;
  filterbank.Compute(input, &output);

  ASSERT_EQ(output.size(), kChannelCount);

  for (int i = 0; i < kChannelCount; ++i) {
    float t = output[i];
    EXPECT_FALSE(std::isnan(t) || std::isinf(t));
  }

  // Golden values for min_frequency=0.0 from mfcc_mel.py via
  // http://colab/v2/notebook#fileId=0B4TJPzYpfSM2RDY3MWk0bEFSdFE .
  std::vector<double> expected = {
    6.55410799, 9.56411605, 13.01286477, 16.57608704, 20.58962488,
    25.13380881, 30.52101218, 36.27805982, 43.40116347, 51.30065013,
    60.04552778, 70.85208474, 82.60955902, 96.41872603, 112.26929653,
    130.46661761, 151.28700221, 175.39139009, 202.84483315, 234.63080493};
  for (int i = 0; i < kChannelCount; ++i) {
    EXPECT_NEAR(output[i], expected[i], 1e-4);
  }
}

TEST(MelFilterbankTest, UpperEdgeAtFftBintIsOk) {
  // Test for bug where the upper frequency limit falls on an FFT bin. This
  // is a corner case where the band mapper is ambiguous but should have a zero
  // weight regardless.
  // Given an FFT size of 512.
  const int kSampleCount = 257;
  constexpr int kChannelCount = 40;
  // And a sample rate of 16kHz.
  constexpr int kSampleRateHz = 16000;
  constexpr double kLowerFrequencyLimit = 125.0;
  // Then the frequency resolution is 16kHz / 512 = 31.25Hz.
  // And the upper frequency limit is a multiple of it: 7.5kHz / 31.25Hz = 240.
  const double kUpperFrequencyLimit = 7500.0;

  MelFilterbank filterbank;
  filterbank.Initialize(kSampleCount, kSampleRateHz, kChannelCount,
                        kLowerFrequencyLimit, kUpperFrequencyLimit);

  std::vector<double> input(kSampleCount);
  std::vector<double> output;
  filterbank.Compute(input, &output);

  ASSERT_EQ(output.size(), kChannelCount);
}

TEST(MelFilterbankTest, InverseIsCloseToOriginal) {
  MelFilterbank filterbank;

  const int kFftLength = 513;
  const int kChannelCount = 20;
  constexpr double kMaxFractionalError = 0.08;

  std::vector<double> mel_filterbank(kChannelCount, 0.0);
  std::vector<double> estimated_squared_magnitude_fft;
  std::vector<double> recomputed_mel_filterbank;

  filterbank.Initialize(kFftLength,
                        /*sample_rate=*/22050, kChannelCount,
                        /*lower_frequency_limit=*/20.0,
                        /*upper_frequency_limit=*/4000.0);

  // Test with all mel bins equal to zero.
  filterbank.EstimateInverse(mel_filterbank, &estimated_squared_magnitude_fft);
  filterbank.Compute(estimated_squared_magnitude_fft,
                     &recomputed_mel_filterbank);
  EXPECT_THAT(recomputed_mel_filterbank, Each(0.0));

  // Generate array of smoothed random mel values.
  absl::BitGen gen;
  for (int i = 1; i < kChannelCount - 1; ++i) {
    mel_filterbank[i] = absl::Uniform<double>(gen, 0.0, 1.0);
  }
  SmoothVector(SmootherCoefficientFromScale(2.0f), &mel_filterbank);

  // Test original mel values vs recomputed mel values. Estimated FFT values
  // tend to smooth out at higher frequencies, due to the nature of a mel
  // filterbank. This makes setting an accuracy threshold quite tricky.
  // Recomputed mel values, however, should be very close to the original ones.
  // The recomputed values will still exhibit a smoothed behavior (albiet less
  // intensely), so error is calculated across all channels and not per channel.
  filterbank.EstimateInverse(mel_filterbank, &estimated_squared_magnitude_fft);
  filterbank.Compute(estimated_squared_magnitude_fft,
                     &recomputed_mel_filterbank);

  double absolute_error_sum = 0.0;
  double mel_channel_sum = 0.0;
  for (int i = 0; i < kChannelCount; ++i) {
    absolute_error_sum +=
        std::abs(mel_filterbank[i] - recomputed_mel_filterbank[i]);
    mel_channel_sum += mel_filterbank[i];
  }
  EXPECT_LT(absolute_error_sum, kMaxFractionalError * mel_channel_sum);
}

}  // namespace
}  // namespace audio_dsp

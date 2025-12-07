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

#include "audio/linear_filters/perceptual_filter_design.h"

#include "audio/dsp/decibels.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {

using testing::UnorderedElementsAre;

namespace {

float GainAtDC(const FilterPolesAndZeros& zpk) {
  std::complex<double> zero = std::complex<double>(0.0, 0);
  return std::abs(zpk.Eval(zero));
}

float GainAtOneKHz(const FilterPolesAndZeros& zpk) {
  std::complex<double> one_khz =
        std::complex<double>(0.0, 2 * M_PI * 1000);
  return std::abs(zpk.Eval(one_khz));
}

TEST(PerceptualLoudnessFilterDesignTest, AWeightingPZTest) {
  {
    constexpr float kSampleRate = 48000;  // Nyquist = 150796 rads/sec.
    FilterPolesAndZeros zpk = PerceptualLoudnessFilterPolesAndZeros(
        kAWeighting, kSampleRate);
    EXPECT_THAT(zpk.GetRealPoles(), UnorderedElementsAre(
        -129.4, -129.4, -676.4, -4636.0, -76655.0, -76655.0));
    EXPECT_THAT(zpk.GetRealZeros(), UnorderedElementsAre(0, 0, 0, 0));
    EXPECT_TRUE(zpk.GetConjugatedZeros().empty());
    EXPECT_TRUE(zpk.GetConjugatedPoles().empty());
    EXPECT_NEAR(GainAtOneKHz(zpk), 1.0, 1e-2);
    EXPECT_NEAR(GainAtDC(zpk), 0.0, 1e-5);
  }
  // The highest poles get dropped because they are greater than Nyquist.
  for (float sample_rate : {24000, 16000, 8000}) {
    FilterPolesAndZeros zpk = PerceptualLoudnessFilterPolesAndZeros(
        kAWeighting, sample_rate);
    EXPECT_THAT(zpk.GetRealPoles(), UnorderedElementsAre(
        -129.4, -129.4, -676.4, -4636.0));
    EXPECT_THAT(zpk.GetRealZeros(), UnorderedElementsAre(0, 0, 0, 0));
    EXPECT_TRUE(zpk.GetConjugatedZeros().empty());
    EXPECT_TRUE(zpk.GetConjugatedPoles().empty());
    EXPECT_NEAR(GainAtOneKHz(zpk), 1.0, 1e-2);
    EXPECT_NEAR(GainAtDC(zpk), 0.0, 1e-5);
  }
}

TEST(PerceptualLoudnessFilterDesignTest, BWeightingPZTest) {
  {
    constexpr float kSampleRate = 48000;  // Nyquist = 150796 rads/sec.
    FilterPolesAndZeros zpk = PerceptualLoudnessFilterPolesAndZeros(
        kBWeighting, kSampleRate);
    EXPECT_THAT(zpk.GetRealPoles(), UnorderedElementsAre(
        -129.4, -129.4, -996.9, -76655.0, -76655.0));
    EXPECT_THAT(zpk.GetRealZeros(), UnorderedElementsAre(0, 0, 0));
    EXPECT_TRUE(zpk.GetConjugatedZeros().empty());
    EXPECT_TRUE(zpk.GetConjugatedPoles().empty());
    EXPECT_NEAR(GainAtOneKHz(zpk), 1.0, 1e-2);
    EXPECT_NEAR(GainAtDC(zpk), 0.0, 1e-5);
  }
  // The highest poles get dropped because they are greater than Nyquist.
  for (float sample_rate : {24000, 16000, 8000}) {
    FilterPolesAndZeros zpk = PerceptualLoudnessFilterPolesAndZeros(
        kBWeighting, sample_rate);
    EXPECT_THAT(zpk.GetRealPoles(), UnorderedElementsAre(
        -129.4, -129.4, -996.9));
    EXPECT_THAT(zpk.GetRealZeros(), UnorderedElementsAre(0, 0, 0));
    EXPECT_TRUE(zpk.GetConjugatedZeros().empty());
    EXPECT_TRUE(zpk.GetConjugatedPoles().empty());
    EXPECT_NEAR(GainAtOneKHz(zpk), 1.0, 1e-2);
    EXPECT_NEAR(GainAtDC(zpk), 0.0, 1e-5);
  }
}

TEST(PerceptualLoudnessFilterDesignTest, CWeightingPZTest) {
  {
    constexpr float kSampleRate = 48000;  // Nyquist = 150796 rads/sec.
    FilterPolesAndZeros zpk = PerceptualLoudnessFilterPolesAndZeros(
        kCWeighting, kSampleRate);
    EXPECT_THAT(zpk.GetRealPoles(), UnorderedElementsAre(
        -129.4, -129.4, -76655.0, -76655.0));
    EXPECT_THAT(zpk.GetRealZeros(), UnorderedElementsAre(0, 0));
    EXPECT_TRUE(zpk.GetConjugatedZeros().empty());
    EXPECT_TRUE(zpk.GetConjugatedPoles().empty());
    EXPECT_NEAR(GainAtOneKHz(zpk), 1.0, 1e-2);
    EXPECT_NEAR(GainAtDC(zpk), 0.0, 1e-5);
  }
  for (float sample_rate : {24000, 16000, 8000}) {
    FilterPolesAndZeros zpk = PerceptualLoudnessFilterPolesAndZeros(
        kCWeighting, sample_rate);
    EXPECT_THAT(zpk.GetRealPoles(), UnorderedElementsAre(-129.4, -129.4));
    EXPECT_THAT(zpk.GetRealZeros(), UnorderedElementsAre(0, 0));
    EXPECT_TRUE(zpk.GetConjugatedZeros().empty());
    EXPECT_TRUE(zpk.GetConjugatedPoles().empty());
    EXPECT_NEAR(GainAtOneKHz(zpk), 1.0, 1e-2);
    EXPECT_NEAR(GainAtDC(zpk), 0.0, 1e-5);
  }
}

TEST(PerceptualLoudnessFilterDesignTest, DWeightingPZTest) {
  for (float sample_rate : {48000, 24000, 16000, 8000}) {
    FilterPolesAndZeros zpk = PerceptualLoudnessFilterPolesAndZeros(
        kDWeighting, sample_rate);
    EXPECT_THAT(zpk.GetRealPoles(), UnorderedElementsAre(-1776.3, -7288.5));
    EXPECT_THAT(zpk.GetRealZeros(), UnorderedElementsAre(0));
    EXPECT_EQ(zpk.GetConjugatedZeros()[0],
              std::complex<double>(-3266, 5505.2));
    EXPECT_EQ(zpk.GetConjugatedPoles()[0],
              std::complex<double>(-10757, 16512.02));
    EXPECT_NEAR(GainAtOneKHz(zpk), 1.0, 1e-2);
    EXPECT_NEAR(GainAtDC(zpk), 0.0, 1e-5);
  }
}

TEST(PerceptualLoudnessFilterDesignTest, RlbWeightingPZTest) {
  for (float sample_rate : {48000, 24000, 16000, 8000}) {
    FilterPolesAndZeros zpk = PerceptualLoudnessFilterPolesAndZeros(
        kRlbWeighting, sample_rate);
    EXPECT_THAT(zpk.GetRealPoles(), UnorderedElementsAre(-240, -240));
    EXPECT_THAT(zpk.GetRealZeros(), UnorderedElementsAre(0, 0));
    EXPECT_TRUE(zpk.GetConjugatedZeros().empty());
    EXPECT_TRUE(zpk.GetConjugatedPoles().empty());
    EXPECT_NEAR(GainAtOneKHz(zpk), 1.0, 1e-2);
    EXPECT_NEAR(GainAtDC(zpk), 0.0, 1e-5);
  }
}

TEST(PerceptualLoudnessFilterDesignTest, KWeightingPZTest) {
  for (float sample_rate : {48000, 24000, 16000, 8000}) {
    FilterPolesAndZeros zpk = PerceptualLoudnessFilterPolesAndZeros(
        kKWeighting, sample_rate);
    EXPECT_THAT(zpk.GetRealPoles(), UnorderedElementsAre(-240, -240));
    EXPECT_THAT(zpk.GetRealZeros(), UnorderedElementsAre(0, 0));
    EXPECT_EQ(zpk.GetConjugatedZeros()[0],
              std::complex<double>(-5943.129, 5976.7400));
    EXPECT_EQ(zpk.GetConjugatedPoles()[0],
              std::complex<double>(-7471.63, 7534.19));
    EXPECT_NEAR(std::abs(zpk.Eval({0, 2 * M_PI * 500})), 1.0, 1e-3);
    EXPECT_NEAR(GainAtDC(zpk), 0.0, 1e-5);
  }
}

// Make sure the rolloff is as expected for standard audio rates and at
// highly reduced rates.
TEST(PerceptualLoudnessFilterDesignTest, DiscretizeTest) {
  for (PerceptualWeightingType weighting : {kAWeighting, kBWeighting,
                                            kCWeighting, kDWeighting,
                                            kRlbWeighting, kKWeighting}) {
    float sample_rate = 48000;
    BiquadFilterCascadeCoefficients coeffs =
        PerceptualLoudnessFilterCoefficients(weighting, sample_rate);
    EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(0, sample_rate), 0.0, 1e-3);
    if (weighting != kKWeighting) {
      EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(1000, sample_rate),
                  1.0, 2e-3);
    } else {
      // K Weighting is approximately unity gain at 500 Hz.
      EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(500, sample_rate),
                  1.0, 2e-3);
    }
    float high_frequency_gain;
    if (weighting == kRlbWeighting) {
      high_frequency_gain = 1.0;
    } else if (weighting == kKWeighting) {
      // 4dB gain at high frequencies.
      high_frequency_gain = audio_dsp::DecibelsToAmplitudeRatio(4.0);
    } else {
      high_frequency_gain = 0.0;
    }
    EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(sample_rate / 2, sample_rate),
                high_frequency_gain, 1e-3);
  }
  // Check the rolloff of filters who don't have all of their upper poles/zeros.
  for (PerceptualWeightingType weighting : {kAWeighting, kBWeighting,
                                            kCWeighting, kRlbWeighting}) {
    float sample_rate = 16000;
    BiquadFilterCascadeCoefficients coeffs =
        PerceptualLoudnessFilterCoefficients(weighting, sample_rate);
    EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(0, sample_rate), 0.0, 1e-3);
    EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(1000, sample_rate), 1.0, 1e-1);
    // At high frequencies, these filters don't roll off (because high
    // frequency isn't actually near the top of the human hearing range in this
    // case).
    EXPECT_NEAR(coeffs.GainMagnitudeAtFrequency(sample_rate / 2, sample_rate),
                1.0, 0.3);
  }
}

}  // namespace
}  // namespace linear_filters

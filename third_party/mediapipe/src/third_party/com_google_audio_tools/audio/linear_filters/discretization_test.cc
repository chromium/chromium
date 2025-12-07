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

#include "audio/linear_filters/discretization.h"

#include "gtest/gtest.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {
namespace {

TEST(DiscretizationTest, TimeConstantToCoeffTest) {
  constexpr float kTimeConstantSeconds = 0.02f;
  constexpr float kSampleRateHz = 16000.0f;
  constexpr float kTimeConstantSamples = kTimeConstantSeconds * kSampleRateHz;
  float alpha = FirstOrderCoefficientFromTimeConstant(kTimeConstantSeconds,
                                                      kSampleRateHz);
  float input = 0.0f;
  float output = 1.0f;
  int num_samples = 0;
  // Measure the time it takes the function to reach 1 / e  (one time constant).
  while (output > 1 / std::exp(1.0f)) {
    output = alpha * input + (1 - alpha) * output;
    ++num_samples;
  }
  EXPECT_EQ(num_samples, std::ceil(kTimeConstantSamples));
}

// Verify that bilinear mapping from s to z works as in MATLAB.
TEST(DiscretizationTest, DiscretizeCoefficientsBilinearTest) {
  double sample_rate_hz = 100;
  double match_frequency_hz = 20;
  BiquadFilterCoefficients coeffs = BilinearTransform(
      {0.000031662869888, 0.001989436788649, 1.000000000000000},
      {0.000063325739776, 0.003978873577297, 1.000000000000000},
      sample_rate_hz,
      match_frequency_hz);
  EXPECT_NEAR(coeffs.b[0], 0.63956273844, 1e-9);
  EXPECT_NEAR(coeffs.b[1], 0.02946806065, 1e-9);
  EXPECT_NEAR(coeffs.b[2], 0.44747110847, 1e-9);
  EXPECT_NEAR(coeffs.a[0], 1.0, 1e-9);
  EXPECT_NEAR(coeffs.a[1], -0.49931483247, 1e-9);
  EXPECT_NEAR(coeffs.a[2], 0.61581674005, 1e-9);
}

TEST(DiscretizationTest, DiscretizePolesZerosTest) {
  FilterPolesAndZeros zpk_analog;
  constexpr float kSampleRateHz = 10.0;
  zpk_analog.AddZero(2.0);
  zpk_analog.AddPole(-5.0);
  FilterPolesAndZeros zpk_digital =
      BilinearTransform(zpk_analog, kSampleRateHz);
  EXPECT_NEAR(zpk_digital.GetRealZeros()[0],
              (2 + 2 / kSampleRateHz) / (2 - 2 / kSampleRateHz), 1e-4);
  EXPECT_NEAR(zpk_digital.GetRealPoles()[0],
              (2 - 5 / kSampleRateHz) / (2 + 5 / kSampleRateHz), 1e-4);
  EXPECT_NEAR(std::abs(zpk_digital.Eval(1)),
              std::abs(zpk_analog.Eval(0)), 1e-6);
}

}  // namespace
}  // namespace linear_filters

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

#include "audio/dsp/signal_generator.h"

#include <cmath>
#include <vector>

#include "audio/dsp/testing_util.h"
#include "audio/dsp/types.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {
namespace {

using ::testing::Each;
using ::testing::Le;
using ::testing::Ge;

static constexpr float kSampleRate = 48000;
static constexpr float kSinFrequency = 500;
static constexpr float kNumSamples = 1000;

// Get the name of Type as a string.
template <typename Type>
std::string GetTypeName() {
  return util::Demangle(typeid(Type).name());
}

std::vector<float> GenerateBasicSineWave(int num_samples, float sample_rate,
                                         float frequency, float amplitude) {
  std::vector<float> result(num_samples);
  const double radians_per_sample = (2.0 * M_PI * frequency) / sample_rate;
  for (int i = 0; i < num_samples; ++i) {
    result[i] = amplitude * std::sin(radians_per_sample * i);
  }
  return result;
}

template <typename TypeParam>
class SignalGeneratorTypedTest : public ::testing::Test {};

// Different arithmetic ValueType types to test.
typedef ::testing::Types<float, double, int16, int32, int64> TestTypes;
TYPED_TEST_SUITE(SignalGeneratorTypedTest, TestTypes);

// Test SignalGenerator for different template args.
TYPED_TEST(SignalGeneratorTypedTest, GenerateSine) {
  SCOPED_TRACE("ValueType: " + GetTypeName<TypeParam>());
  TypeParam amplitude = std::is_floating_point<TypeParam>::value ? 1 : 10000;
  std::vector<TypeParam> sine_wave = GenerateSine<TypeParam>(
      kNumSamples, kSampleRate, kSinFrequency, amplitude);

  float tolerance = std::is_floating_point<TypeParam>::value ? 1e-5 : 1;
  std::vector<float> expected_sin_wave =
      GenerateBasicSineWave(kNumSamples, kSampleRate, kSinFrequency, amplitude);
  EXPECT_THAT(sine_wave, Each(Le(amplitude)));
  EXPECT_THAT(sine_wave, Each(Ge(-amplitude)));
  EXPECT_THAT(sine_wave, FloatArrayNear(expected_sin_wave, tolerance));
}

TEST(SignalGeneratorTest, GenerateSineEigen) {
  constexpr float kAmplitude = 1;
  Eigen::ArrayXf sine_wave =
      GenerateSineEigen(kNumSamples, kSampleRate, kSinFrequency, kAmplitude);
  std::vector<float> expected_sine_wave = GenerateBasicSineWave(
      kNumSamples, kSampleRate, kSinFrequency, kAmplitude);
  Eigen::Map<Eigen::ArrayXf> expected_sin_wave_array(expected_sine_wave.data(),
                                                     kNumSamples);
  EXPECT_LE(sine_wave.maxCoeff(), kAmplitude);
  EXPECT_GE(sine_wave.minCoeff(), -kAmplitude);
  EXPECT_TRUE(sine_wave.isApprox(expected_sin_wave_array));
}

// Verifies that the GenerateLinearArrayBroadsideImpulse function correction
// populates only a signal impulse entry for each channel.
TEST(SignalGeneratorTest, GenerateLinearArrayBroadsideImpulse) {
  constexpr int kNumChannels = 10;
  Eigen::MatrixXf data = GenerateLinearArrayBroadsideImpulse<Eigen::MatrixXf>(
      kNumChannels, kNumSamples, 0);
  for (int row = 0; row < data.rows(); ++row) {
    for (int col = 0; col < data.cols(); ++col) {
      if (col == 0) {
        ASSERT_EQ(1, data(row, col));
      } else {
        ASSERT_EQ(0, data(row, col));
      }
    }
  }
}

// Verifies that the GenerateLinearArrayBroadsideImpulse fails a ABSL_CHECK condition
// if given invalid argument values.
TEST(SignalGeneratorDeathTest, FailedPrecondition) {
  EXPECT_DEATH(
      GenerateLinearArrayBroadsideImpulse<Eigen::MatrixXf>(-10, 10, 10), "");
  EXPECT_DEATH(
      GenerateLinearArrayBroadsideImpulse<Eigen::MatrixXf>(10, -10, 10), "");
  EXPECT_DEATH(
      GenerateLinearArrayBroadsideImpulse<Eigen::MatrixXf>(10, 10, -10), "");
  EXPECT_DEATH(GenerateLinearArrayBroadsideImpulse<Eigen::MatrixXf>(10, 10, 12),
               "");
}

void TestGaussianNoise(int n, float mean, float stddev) {
  // We test the sample mean and variance at 5 sigma to make this test fail with
  // probability ~1e-6.
  const float kToleranceSigma = 5.0f;
  // Slack for error in summation.
  const float eps = std::numeric_limits<float>::epsilon();
  const float variance = stddev * stddev;
  const Eigen::ArrayXf noise = GenerateWhiteGaussianNoise(n, mean, stddev);
  EXPECT_EQ(noise.size(), n);
  const float sample_mean = noise.mean();
  const float sample_variance = (noise - sample_mean).square().sum() / (n - 1);
  // Variance of the sample mean is variance / n  (law of large numbers).
  EXPECT_NEAR(sample_mean, mean,
              kToleranceSigma * std::sqrt(variance / n) + 2 * n * eps);
  // Variance of the sample_variance is 2 * variance^2 / (n-1), see
  // https://en.wikipedia.org/wiki/Variance#Sample_variance
  EXPECT_NEAR(sample_variance, variance,
              kToleranceSigma * std::sqrt(2 * variance * variance / (n - 1)) +
                  2 * n * eps)
      << "n = " << n << ", mean = " << mean << ", stddev = " << stddev;
}

// Verifies that GenerateWhiteGaussianNoise populates an array
// of the expected length with numbers that have the expected mean
// and variance.
TEST(SignalGeneratorTest, GenerateWhiteGaussianNoise) {
  for (int n : {100, 1000, 10000}) {
    for (float mean : {0.0f, 0.1f, -1.0f, 10.0f}) {
      for (float stddev : {0.0f, 0.1f, 1.0f, 10.0f}) {
        TestGaussianNoise(n, mean, stddev);
      }
    }
  }
}

}  // namespace
}  // namespace audio_dsp

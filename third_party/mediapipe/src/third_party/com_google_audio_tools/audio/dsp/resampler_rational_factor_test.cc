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

#include "audio/dsp/resampler_rational_factor.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <random>
#include <type_traits>

#include "audio/dsp/signal_vector_util.h"
#include "audio/dsp/testing_util.h"
#include "audio/dsp/types.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {
namespace {

using ::std::complex;
using ::testing::Each;
using ::testing::Lt;

// Test properties of ResamplingKernel over various sample rates and radii.
TEST(ResamplerRationalFactorTest, ResamplingKernel) {
  constexpr double kKaiserBeta = 6.0;
  for (double input_sample_rate : {12000, 16000, 32000, 44100, 48000}) {
    for (double output_sample_rate : {12000, 16000, 32000, 44100, 48000}) {
      SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                   input_sample_rate, output_sample_rate));
      for (double radius : {5, 17}) {
        if (input_sample_rate > output_sample_rate) {
          radius *= input_sample_rate / output_sample_rate;
        }
        SCOPED_TRACE(absl::StrFormat("radius: %g input samples", radius));
        const double cutoff =
            0.45 * std::min(input_sample_rate, output_sample_rate);
        DefaultResamplingKernel kernel(input_sample_rate, output_sample_rate,
                                       radius, cutoff, kKaiserBeta);
        ASSERT_TRUE(kernel.Valid());
        EXPECT_EQ(radius, kernel.radius());
        // The kernel should be zero outside of [-radius, +radius].
        EXPECT_FLOAT_EQ(0.0, kernel.Eval(radius + 0.1));

        std::vector<double> samples;
        constexpr double dx = 0.02;
        for (double x = dx; x <= radius; x += dx) {
          samples.push_back(kernel.Eval(x));
        }
        // The kernel has a strict maximum at x = 0.
        EXPECT_THAT(samples, Each(Lt(kernel.Eval(0))));
        // Check that the resampling kernel sums to one.
        EXPECT_NEAR(dx * (kernel.Eval(0) + 2 * Sum(samples)), 1.0, 0.01);
      }
    }
  }
}

TEST(ResamplerRationalFactorTest, ResamplingKernelInvalid) {
  DefaultResamplingKernel kernel(0, 0, 0, 0, 0);
  ASSERT_FALSE(kernel.Valid());
}

TEST(ResamplerRationalFactorTest, ResamplingConstruction) {
  DefaultResamplingKernel kernel(48000.0, 44100.0);
  // 48k / 44.1k = 160 / 147.
  RationalFactorResampler<float> resampler1(kernel, 500);
  RationalFactorResampler<float> resampler2(48000, 44100);

  EXPECT_EQ(resampler1.factor_numerator(), resampler2.factor_numerator());
  EXPECT_EQ(resampler1.factor_denominator(), resampler2.factor_denominator());
}

namespace {
// Finds the largest correlation between `output` and `chirp` over lags spaced
// `search_step_s` apart with magnitude up to `search_radius_s`.
template <typename Fun>
double FindCorrelationPeak(const Eigen::ArrayXf& output, Fun chirp,
                           double output_sample_rate,
                           double search_radius_s,
                           double search_step_s) {
  const double output_duration_s = output.size() / output_sample_rate;
  auto window = [output_duration_s](double t) {
    return std::sin(M_PI * std::min(std::max(
        t / output_duration_s, 0.0), 1.0)); };

  const int search_radius_points =
      static_cast<int>(std::ceil(search_radius_s / search_step_s));
  double peak_lag_s = -std::numeric_limits<double>::infinity();
  double peak_value = -std::numeric_limits<double>::infinity();
  for (int k = -search_radius_points; k <= search_radius_points; ++k) {
    const double lag_s = k * search_step_s;
    double correlation = 0.0;
    for (int i = 0; i < output.size(); ++i) {
      const double t = i / output_sample_rate;
      const double windowed_output = window(t) * output[i];
      const double windowed_chirp = window(t + lag_s) * chirp(t + lag_s);
      correlation += windowed_output * windowed_chirp;
    }
    if (correlation > peak_value) {
      peak_lag_s = lag_s;
      peak_value = correlation;
    }
  }
  return peak_lag_s;
}
}  // namespace

TEST(ResamplerRationalFactorTest, TimeAlignment) {
  for (double input_sample_rate : {16000, 32000, 44100, 48000}) {
    for (double output_sample_rate : {16000, 32000, 44100, 48000}) {
      SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                   input_sample_rate, output_sample_rate));
      const double max_frequency = 0.45 * std::min<double>(input_sample_rate,
                                                           output_sample_rate);
      constexpr double kDurationSeconds = 0.015;
      auto chirp = [max_frequency, kDurationSeconds](double t) {
        return std::sin(M_PI * max_frequency * t * t / kDurationSeconds); };

      Eigen::ArrayXf input(
          1 + static_cast<int>(kDurationSeconds * input_sample_rate));
      for (int i = 0; i < input.size(); ++i) {
        input[i] = chirp(i / input_sample_rate);
      }

      const int factor_numerator = input_sample_rate;
      const int factor_denominator = output_sample_rate;
      RationalFactorResampler<float> resampler(factor_numerator,
                                               factor_denominator);

      // When using `Reset()`, the input and output streams are time aligned. We
      // check this by correlating the output with the continuous-time chirp.
      resampler.Reset();
      ASSERT_TRUE(resampler.Valid());
      Eigen::ArrayXf output;
      resampler.ProcessSamplesEigen(input, &output);

      double search_radius_s = 2.1 * resampler.radius() / input_sample_rate;
      double search_step_s = 0.25 / max_frequency;
      double output_stream_delay_s = -FindCorrelationPeak(
          output, chirp, output_sample_rate, search_radius_s, search_step_s);
      ASSERT_NEAR(output_stream_delay_s, 0.0, search_step_s / 2);

      // When using `ResetFullyPrimed()`, the output stream is delayed by
      // `radius()` input samples.
      resampler.ResetFullyPrimed();
      ASSERT_TRUE(resampler.Valid());
      resampler.ProcessSamplesEigen(input, &output);

      output_stream_delay_s = -FindCorrelationPeak(
          output, chirp, output_sample_rate, search_radius_s, search_step_s);
      ASSERT_NEAR(output_stream_delay_s, resampler.radius() / input_sample_rate,
                  search_step_s / 2);
    }
  }
}

// When using `ResetFullyPrimed()`, and provided that the input buffer size is a
// multiple of `factor_numerator / gcd(factor_numerator, factor_denominator)`,
// the output size is always exactly (input.size() * factor_denominator) /
// factor_numerator.
TEST(ResamplerRationalFactorTest, FullyPrimed) {
  for (double input_sample_rate : {16000, 32000, 44100, 48000}) {
    for (double output_sample_rate : {16000, 32000, 44100, 48000}) {
      SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                   input_sample_rate, output_sample_rate));
      int factor_numerator = input_sample_rate;
      int factor_denominator = output_sample_rate;
      int gcd = GreatestCommonDivisor(factor_numerator, factor_denominator);

      int reduced_numerator = factor_numerator / gcd;
      int reduced_denominator = factor_denominator / gcd;
      RationalFactorResampler<float> resampler(reduced_numerator,
                                               reduced_denominator);

      Eigen::ArrayXf input = Eigen::ArrayXf::Zero(reduced_numerator);
      Eigen::ArrayXf output;

      resampler.ResetFullyPrimed();
      for (int i = 0; i < 3; ++i) {
        resampler.ProcessSamplesEigen(input, &output);
        const int expected_output_size = reduced_denominator;
        ASSERT_EQ(output.size(), expected_output_size);
      }
    }
  }
}

template <typename ResamplerType>
class ResamplerRationalFactorTypedTest : public ::testing::Test {
 protected:
  ResamplerRationalFactorTypedTest(): rng_(0 /* seed */) {}

  std::mt19937 rng_;
};

typedef ::testing::Types<float, double, complex<float>, complex<double>>
    TestResamplerTypes;
TYPED_TEST_SUITE(ResamplerRationalFactorTypedTest, TestResamplerTypes);

// Implement resampling directly according to
//   x'[m] = x(m/F') = sum_n x[n] h(m F/F' - n),
// where h is the resampling kernel, F is the input sample rate, and F' is the
// output sample rate.
template <typename ValueType>
std::vector<ValueType> ReferenceResampling(
    const ResamplingKernel& kernel, const std::vector<ValueType>& input) {
  const double factor =
      kernel.input_sample_rate() / kernel.output_sample_rate();
  std::vector<ValueType> output;
  for (int m = 0;; ++m) {
    const int n_first = std::round(m * factor - kernel.radius());
    const int n_last = std::round(m * factor + kernel.radius());
    ABSL_CHECK_EQ(kernel.Eval(m * factor - (n_first - 1)), 0.0);
    ABSL_CHECK_EQ(kernel.Eval(m * factor - (n_last + 1)), 0.0);
    if (n_last >= input.size()) {
      break;
    }
    ValueType sum = ValueType(0);
    for (int n = n_first; n <= n_last; ++n) {
      sum += (n < 0 ? ValueType(0) : input[n]) *
          static_cast<ValueType>(kernel.Eval(m * factor - n));
    }
    output.push_back(sum);
  }
  return output;
}

// Generate a random value of unit normal distribution of type ValueType.
template <typename ValueType>
ValueType GenerateRandomSample(std::mt19937* rng);

template <>
float GenerateRandomSample(std::mt19937* rng) {
  return std::normal_distribution<float>(0.0, 1.0)(*rng);
}

template <>
double GenerateRandomSample(std::mt19937* rng) {
  return std::normal_distribution<double>(0.0, 1.0)(*rng);
}

template <>
complex<float> GenerateRandomSample(std::mt19937* rng) {
  return {GenerateRandomSample<float>(rng), GenerateRandomSample<float>(rng)};
}

template <>
complex<double> GenerateRandomSample(std::mt19937* rng) {
  return {GenerateRandomSample<double>(rng), GenerateRandomSample<double>(rng)};
}

// Generate a vector<ValueType> of unit normally-distributed values.
template <typename ValueType>
std::vector<ValueType> GenerateRandomVector(int num_samples,
                                            std::mt19937* rng) {
  std::vector<ValueType> result(num_samples);
  for (ValueType& sample : result) {
    sample = GenerateRandomSample<ValueType>(rng);
  }
  return result;
}

// Compare RationalFactorResampler with ReferenceResampling().
TYPED_TEST(ResamplerRationalFactorTypedTest, CompareWithReferenceResampler) {
  typedef TypeParam ValueType;
  constexpr int kMaxDenominator = 500;
  constexpr int kNumSamples = 50;
  constexpr double kKaiserBeta = 6.0;
  std::vector<ValueType> input =
      GenerateRandomVector<ValueType>(kNumSamples, &this->rng_);
  for (double input_sample_rate : {12000, 16000, 32000, 44100, 48000}) {
    for (double output_sample_rate : {12000, 16000, 32000, 44100, 48000}) {
      if (input_sample_rate == output_sample_rate) {
        continue;
      }
      SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                   input_sample_rate, output_sample_rate));
      for (double radius : {4, 5, 17}) {
        if (input_sample_rate > output_sample_rate) {
          radius *= input_sample_rate / output_sample_rate;
        }
        SCOPED_TRACE(absl::StrFormat("radius: %g input samples", radius));
        const double cutoff =
            0.45 * std::min(input_sample_rate, output_sample_rate);
        DefaultResamplingKernel kernel(input_sample_rate, output_sample_rate,
                                       radius, cutoff, kKaiserBeta);

        RationalFactorResampler<ValueType> resampler(kernel, kMaxDenominator);
        ASSERT_TRUE(resampler.Valid());

        std::vector<ValueType> output;
        resampler.ProcessSamples(input, &output);

        std::vector<ValueType> expected = ReferenceResampling(kernel, input);

        // Allow output length to differ by up to two samples.
        ASSERT_NEAR(output.size(), expected.size(), 2);
        if (output.size() > expected.size()) {
          output.erase(output.begin() + expected.size(), output.end());
        } else if (expected.size() > output.size()) {
          expected.erase(expected.begin() + output.size(), expected.end());
        }
        EXPECT_THAT(output, FloatArrayNear(expected, 1e-4));
      }
    }
  }
}

// A simple kernel for piecewise linear interpolation.
class LinearResamplingKernel: public ResamplingKernel {
 public:
  LinearResamplingKernel(double input_sample_rate, double output_sample_rate)
      : ResamplingKernel(input_sample_rate, output_sample_rate, 1.0) {}

  double Eval(double x) const override {
    return std::max(1.0 - std::abs(x), 0.0);
  }

  bool Valid() const override { return true; }
};

// Test resampling using a non-default ResamplingKernel.
TYPED_TEST(ResamplerRationalFactorTypedTest, LinearResamplingKernel) {
  typedef TypeParam ValueType;
  constexpr int kMaxDenominator = 500;
  constexpr int kNumSamples = 50;
  constexpr double kInputSampleRate = 12000;
  constexpr double kOutputSampleRate = 44100;
  std::vector<ValueType> input =
      GenerateRandomVector<ValueType>(kNumSamples, &this->rng_);
  LinearResamplingKernel kernel(kInputSampleRate, kOutputSampleRate);
  RationalFactorResampler<ValueType> resampler(kernel, kMaxDenominator);
  ASSERT_TRUE(resampler.Valid());

  std::vector<ValueType> output;
  resampler.ProcessSamples(input, &output);

  std::vector<ValueType> expected = ReferenceResampling(kernel, input);

  // Allow output length to differ by up to two samples.
  ASSERT_NEAR(output.size(), expected.size(), 2);
  if (output.size() > expected.size()) {
    output.erase(output.begin() + expected.size(), output.end());
  } else if (expected.size() > output.size()) {
    expected.erase(expected.begin() + output.size(), expected.end());
  }
  EXPECT_THAT(output, FloatArrayNear(expected, 1e-4));
}

// Resampling a sine wave should produce again a sine wave.
TYPED_TEST(ResamplerRationalFactorTypedTest, ResampleSineWave) {
  typedef TypeParam ValueType;
  constexpr double kFrequency = 1100.7;
  for (double input_sample_rate : {12000, 16000, 32000, 44100, 48000}) {
    for (double output_sample_rate : {12000, 16000, 32000, 44100, 48000}) {
      SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                   input_sample_rate, output_sample_rate));
      DefaultResamplingKernel kernel(input_sample_rate, output_sample_rate);
      int factor_numerator = input_sample_rate;
      int factor_denominator = output_sample_rate;
      RationalFactorResampler<ValueType> resampler(
          kernel, factor_numerator, factor_denominator);
      ASSERT_TRUE(resampler.Valid());

      std::vector<ValueType> input;
      ComputeSineWaveVector(kFrequency, input_sample_rate, 0.0, 100, &input);

      std::vector<ValueType> output;
      resampler.Reset();
      ASSERT_TRUE(resampler.Valid());
      resampler.ProcessSamples(input, &output);
      const double expected_duration =
          (input.size() - kernel.radius()) / input_sample_rate;
      const double actual_duration = output.size() / output_sample_rate;
      EXPECT_NEAR(actual_duration, expected_duration, 2.0 / output_sample_rate);

      // We ignore the first few output samples because they depend on input
      // samples at negative times, which are extrapolated as zeros.
      const int kNumToIgnore =
          (kernel.radius() * output_sample_rate) / input_sample_rate + 1;
      ASSERT_GT(output.size(), kNumToIgnore);

      std::vector<ValueType> expected;
      ComputeSineWaveVector(kFrequency, output_sample_rate, 0.0, output.size(),
                            &expected);
      expected.erase(expected.begin(), expected.begin() + kNumToIgnore);
      output.erase(output.begin(), output.begin() + kNumToIgnore);
      EXPECT_THAT(output, FloatArrayNear(expected, 0.1));
    }
  }
}

// Test streaming with blocks of random sizes between 0 and kMaxBlockSize.
TYPED_TEST(ResamplerRationalFactorTypedTest, StreamingRandomBlockSizes) {
  typedef TypeParam ValueType;
  constexpr int kNumSamples = 500;
  constexpr int kMaxBlockSize = 20;
  constexpr int kFactorNumerator = 44100;
  constexpr int kFactorDenominator = 12000;
  DefaultResamplingKernel kernel(kFactorNumerator, kFactorDenominator);
  RationalFactorResampler<ValueType> resampler(
      kernel, kFactorNumerator, kFactorDenominator);
  ASSERT_TRUE(resampler.Valid());

  std::uniform_int_distribution<int> block_size_distribution(0, kMaxBlockSize);
  std::vector<ValueType> output_buffer;
  std::vector<ValueType> input =
      GenerateRandomVector<ValueType>(kNumSamples, &this->rng_);
  resampler.Reset();
  ASSERT_TRUE(resampler.Valid());
  std::vector<ValueType> nonstreaming;
  resampler.ProcessSamples(input, &nonstreaming);

  resampler.Reset();
  ASSERT_TRUE(resampler.Valid());
  std::vector<ValueType> streaming;
  for (int start = 0; start < input.size();) {
    int current_block_size = std::min<int>(block_size_distribution(this->rng_),
                                           input.size() - start);
    std::vector<ValueType> output_block;
    resampler.ProcessSamples(
        std::vector<ValueType>(input.cbegin() + start,
                               input.cbegin() + start + current_block_size),
        &output_buffer);
    VectorAppend(&streaming, output_buffer);
    start += current_block_size;
  }
  EXPECT_THAT(streaming, FloatArrayNear(nonstreaming, 1e-6));
}

TYPED_TEST(ResamplerRationalFactorTypedTest, WorksWithEigenTypes) {
  typedef TypeParam ValueType;
  constexpr int kNumSamples = 500;
  constexpr int kFactorNumerator = 44100;
  constexpr int kFactorDenominator = 12000;
  DefaultResamplingKernel kernel(kFactorNumerator, kFactorDenominator);
  RationalFactorResampler<ValueType> resampler(
      kernel, kFactorNumerator, kFactorDenominator);
  ASSERT_TRUE(resampler.Valid());

  std::vector<ValueType> input =
      GenerateRandomVector<ValueType>(kNumSamples, &this->rng_);
  resampler.Reset();
  ASSERT_TRUE(resampler.Valid());
  std::vector<ValueType> vector_samples;
  resampler.ProcessSamples(input, &vector_samples);
  std::vector<ValueType> vector_flushed;
  resampler.Flush(&vector_flushed);
  // Reset and process same samples as Eigen matrix.
  resampler.Reset();
  ASSERT_TRUE(resampler.Valid());
  {  // Try for matrix types.
    Eigen::Matrix<ValueType, Eigen::Dynamic, 1> eigen_input(kNumSamples);
    for (int i = 0; i < kNumSamples; ++i) {
      eigen_input[i] = input[i];
    }
    Eigen::Matrix<ValueType, Eigen::Dynamic, 1> eigen_samples;
    resampler.ProcessSamplesEigen(eigen_input, &eigen_samples);
    Eigen::Matrix<ValueType, Eigen::Dynamic, 1> eigen_flushed;
    resampler.FlushEigen(&eigen_flushed);

    for (int i = 0; i < vector_samples.size(); ++i) {
      // EXPECT_NEAR doesn't support complex types.
      EXPECT_LT(std::abs(eigen_samples[i] - vector_samples[i]), 1e-6);
    }
    // Make sure flushed samples agree.
    for (int i = 0; i < vector_flushed.size(); ++i) {
      EXPECT_LT(std::abs(eigen_flushed[i] - vector_flushed[i]), 1e-6);
    }
  }
  {  // Try for array types.
    Eigen::Array<ValueType, Eigen::Dynamic, 1> eigen_input(kNumSamples);
    for (int i = 0; i < kNumSamples; ++i) {
      eigen_input[i] = input[i];
    }
    Eigen::Array<ValueType, Eigen::Dynamic, 1> eigen_samples;
    resampler.ProcessSamplesEigen(eigen_input, &eigen_samples);
    Eigen::Array<ValueType, Eigen::Dynamic, 1> eigen_flushed;
    resampler.FlushEigen(&eigen_flushed);

    for (int i = 0; i < vector_samples.size(); ++i) {
      // EXPECT_NEAR doesn't support complex types.
      EXPECT_LT(std::abs(eigen_samples[i] - vector_samples[i]), 1e-6);
    }
    // Make sure flushed samples agree.
    for (int i = 0; i < vector_flushed.size(); ++i) {
      EXPECT_LT(std::abs(eigen_flushed[i] - vector_flushed[i]), 1e-6);
    }
  }
}

TYPED_TEST(ResamplerRationalFactorTypedTest,
           TestProcessSamplesAndFlushSampleCounts) {
  // In this test, we compute how many samples the resampler will output.
  //
  // See RationalFactorResampler::ComputeOutputSizeFromCurrentState() for
  // detailed derivation of the output size estimates.
  typedef TypeParam ValueType;

  constexpr int kMaxBlockSize = 200;
  std::uniform_int_distribution<int> block_size_distribution(0, kMaxBlockSize);

  for (double input_sample_rate : {12000, 16000, 32000, 44100, 48000}) {
    for (double output_sample_rate : {12000, 16000, 32000, 44100, 48000}) {
      SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz.",
                                   input_sample_rate, output_sample_rate));

      DefaultResamplingKernel kernel(input_sample_rate, output_sample_rate);
      RationalFactorResampler<ValueType> resampler(kernel, input_sample_rate,
                                                   output_sample_rate);
      ASSERT_TRUE(resampler.Valid());

      const int num_taps = 2 * resampler.radius() + 1;
      const int factor_numerator = resampler.factor_numerator();
      const int factor_denominator = resampler.factor_denominator();
      const int factor_floor =
          factor_numerator / factor_denominator;  // Integer divide.
      const int phase_step = factor_numerator % factor_denominator;

      std::vector<ValueType> input, output;

      constexpr int kNumBlocks = 10;
      int delayed_input_size = resampler.radius();
      int phase = 0;
      for (int i = 0; i < kNumBlocks; ++i) {
        ASSERT_EQ(resampler.delayed_input_size(), delayed_input_size);
        ASSERT_EQ(resampler.phase(), phase);

        input = GenerateRandomVector<ValueType>(
            block_size_distribution(this->rng_), &this->rng_);
        resampler.ProcessSamples(input, &output);

        int num_expected_output_samples = std::ceil(
            (std::max<int>(delayed_input_size + input.size() - num_taps + 1,
                           0) *
                 factor_denominator -
             phase) /
            static_cast<float>(phase_step + factor_floor * factor_denominator));
        ASSERT_EQ(output.size(), num_expected_output_samples);

        int num_consumed_input_samples =
            (phase + output.size() * phase_step) /
                factor_denominator +  // Integer divide.
            output.size() * factor_floor;

        delayed_input_size +=
            (input.size() - num_consumed_input_samples);
        phase = ((phase + phase_step * output.size()) % factor_denominator);
      }

      resampler.Flush(&output);
      // Flushing uses an input size of num_taps - 1.
      int num_expected_output_samples = std::ceil(
          (std::max<int>(delayed_input_size + (num_taps - 1) - num_taps + 1,
                         0) *
               factor_denominator -
           phase) /
          static_cast<float>(phase_step + factor_floor * factor_denominator));
      ASSERT_EQ(output.size(), num_expected_output_samples);
    }
  }
}

TYPED_TEST(ResamplerRationalFactorTypedTest, TestInvalidResampler) {
  DefaultResamplingKernel kernel(44100, 44100);
  RationalFactorResampler<TypeParam> resampler(kernel, 0, 0);
  ASSERT_FALSE(resampler.Valid());
}

TYPED_TEST(ResamplerRationalFactorTypedTest,
           TestComputeOutputSizeFromCurrentStateDoesntOverflow) {
  // On real data, ComputeOutputSizeFromCurrentState overflowed int32.
  // It internally computes input_size * factor_denominator.  The denominator
  // in this case is 160 (numerator 441).  A single music track input was
  // about 14M samples (5 min at 44 kHz), yielding a product just larger than
  // 2^31.  This caused a call to vector.resize() with a negative argument, and
  // thus a core dump.
  typedef TypeParam ValueType;

  double input_sample_rate = 44100;
  double output_sample_rate = 16000;
  DefaultResamplingKernel kernel(input_sample_rate, output_sample_rate);
  RationalFactorResampler<ValueType> resampler(kernel, input_sample_rate,
                                               output_sample_rate);
  ASSERT_TRUE(resampler.Valid());

  std::vector<ValueType> input, output;

  input.resize(14038500);  // This was the value that triggered the error.

  // If it doesn't dump core, we're good.
  resampler.ProcessSamples(input, &output);
}

}  // namespace
}  // namespace audio_dsp

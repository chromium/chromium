/*
 * Copyright 2021 Google LLC
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

#include "audio/dsp/resampler_q.h"

#include <array>
#include <cmath>
#include <complex>
#include <random>
#include <type_traits>

#include "audio/dsp/number_util.h"
#include "audio/dsp/portable/rational_factor_resampler_kernel.h"
#include "audio/dsp/signal_vector_util.h"
#include "audio/dsp/testing_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"

namespace audio_dsp {
namespace {

// Tested sample rates in Hz.
constexpr std::array<double, 4> kTestRates = {16000.0, 44100.0, 48000.0,
                                              16000.0 * M_SQRT2};

// Compare QResampler output with golden values RationalFactorResampler, to show
// that it matches to high precision. This verifies that QResampler's processing
// is the same as RationalFactorResampler, so that migration to QResampler
// doesn't change how signals are resampled.
TEST(ResamplerQTest, CompareWithGolden) {
  QResampler<float> resampler(44100, 16000);
  ASSERT_TRUE(resampler.Valid());

  std::vector<float> input(
      {-0.0542f, -0.1336f, -0.0352f, -0.1292f, 0.1196f,  -0.1653f, -0.0068f,
       0.95f,    -0.0583f, -0.1578f, 0.2855f,  -0.3011f, -0.2696f, 0.37f,
       -0.409f,  -0.2246f, 0.3803f,  -0.4523f, -0.0187f, 0.18f,    -0.6072f,
       -0.0317f, 0.0191f,  -0.1686f, 0.1505f,  0.1894f,  -0.0442f, 0.0551f,
       0.0682f,  -0.0693f, -0.0842f, -0.2946f, -0.1311f, -0.015f,  -0.0312f,
       0.0047f,  0.0962f,  0.0297f,  0.0347f,  -0.0407f, -0.0246f, -0.1278f,
       0.1066f,  0.086f,   0.0163f,  0.0024f,  -0.2727f, -0.3417f, -0.1738f,
       0.2183f,  0.4465f,  0.3985f,  0.2203f,  -0.0453f, -0.2743f, -0.6718f,
       -0.4199f, 0.0192f,  0.4951f,  0.8771f,  0.6888f,  0.0858f,  -0.6568f,
       -0.9224f, -0.578f,  0.0122f,  0.6705f,  0.8023f,  0.635f,   0.0493f,
       -0.5006f, -0.7678f, -0.4807f, -0.2301f, 0.1717f,  0.3078f,  0.3742f,
       0.2856f,  0.206f,   0.18f,    0.0834f,  -0.2084f, -0.2348f, -0.4778f,
       -0.2182f, -0.0843f, 0.4884f,  0.6593f,  0.4967f,  0.2237f,  -0.2711f,
       -0.5617f, -0.3897f, -0.2427f, -0.0282f, 0.2255f,  -0.0244f, 0.2873f});
  std::vector<float> output;
  resampler.ProcessSamples(input, &output);

  EXPECT_THAT(output,
              FloatArrayNear(std::vector<double>(
                                 {// Expected values computed by
                                  // RationalFactorResampler on 2020-12-07.
                                  -0.0435989, -0.112129,  0.162489,   0.195123,
                                  -0.117406,  -0.0880727, -0.0577896, -0.162459,
                                  -0.0933089, 0.0989708,  0.0301008,  -0.15221,
                                  -0.0835734, 0.0686006,  -0.0291106, 0.0150585,
                                  0.00914149, -0.262734,  0.305897,   0.147136,
                                  -0.547878,  0.497583,   0.241149,   -0.747131,
                                  0.609127,   0.133936,   -0.627896,  0.19975,
                                  0.337692,   0.0184064,  -0.390075}),
                             1e-6));
}

TEST(ResamplerQTest, ResamplingConstruction) {
  QResampler<float> resampler(48000, 44100);
  EXPECT_TRUE(resampler.Valid());
  EXPECT_EQ(resampler.num_channels(), 1);
  EXPECT_EQ(resampler.factor_numerator(), 160);  // 48000 / 44100 = 160 / 147.
  EXPECT_EQ(resampler.factor_denominator(), 147);

  resampler.Init(8000, 44100, 2);
  EXPECT_TRUE(resampler.Valid());
  EXPECT_EQ(resampler.num_channels(), 2);
  EXPECT_EQ(resampler.factor_numerator(), 80);  // 8000 / 44100 = 80 / 441.
  EXPECT_EQ(resampler.factor_denominator(), 441);
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

// Test the resampler's time alignment by resampling a chirp and finding the
// correlation peak in the output.
TEST(ResamplerQTest, TimeAlignment) {
  for (double input_sample_rate : kTestRates) {
    for (double output_sample_rate : kTestRates) {
      if (input_sample_rate == output_sample_rate) {
        continue;
      }

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

      QResampler<float> resampler(input_sample_rate, output_sample_rate);
      ASSERT_TRUE(resampler.Valid());

      // When using `Reset()`, input and output streams are time aligned.
      resampler.Reset();
      Eigen::ArrayXf output;
      resampler.ProcessSamples(input, &output);

      double search_radius_s = 2.1 * resampler.radius() / input_sample_rate;
      double search_step_s = 0.25 / max_frequency;
      double tolerance = search_step_s / 2;
      double output_stream_delay_s = -FindCorrelationPeak(
          output, chirp, output_sample_rate, search_radius_s, search_step_s);
      ASSERT_NEAR(output_stream_delay_s, 0.0, tolerance);

      // When using `ResetFullyPrimed()`, the output stream is delayed by
      // `radius()` input samples.
      resampler.ResetFullyPrimed();
      resampler.ProcessSamples(input, &output);

      output_stream_delay_s = -FindCorrelationPeak(
          output, chirp, output_sample_rate, search_radius_s, search_step_s);
      ASSERT_NEAR(output_stream_delay_s, resampler.radius() / input_sample_rate,
                  tolerance);
    }
  }
}

// When using `ResetFullyPrimed()`, and provided that the input buffer size is a
// multiple of `factor_numerator / gcd(factor_numerator, factor_denominator)`,
// the output size is always exactly (input.size() * factor_denominator) /
// factor_numerator.
TEST(ResamplerQTest, FullyPrimed) {
  for (int input_sample_rate : {16000, 32000, 44100, 48000}) {
    for (int output_sample_rate : {16000, 32000, 44100, 48000}) {
      if (input_sample_rate == output_sample_rate) {
        continue;
      }

      SCOPED_TRACE(absl::StrFormat("Resampling from %dHz to %dHz",
                                   input_sample_rate, output_sample_rate));
      int factor_numerator = input_sample_rate;
      int factor_denominator = output_sample_rate;
      int gcd = GreatestCommonDivisor(factor_numerator, factor_denominator);

      int reduced_numerator = factor_numerator / gcd;
      int reduced_denominator = factor_denominator / gcd;
      QResampler<float> resampler(input_sample_rate, output_sample_rate);

      Eigen::ArrayXf input = Eigen::ArrayXf::Zero(reduced_numerator);
      Eigen::ArrayXf output;

      resampler.ResetFullyPrimed();
      for (int i = 0; i < 3; ++i) {
        resampler.ProcessSamples(input, &output);
        ASSERT_EQ(output.size(), reduced_denominator);
      }
    }
  }
}

// Test NextNumInputToProduce() method, which computes how many input frames
// are needed to produce at least a specified number of output frames.
TEST(ResamplerQTest, NextNumInputToProduce) {
  Eigen::ArrayXf output;

  for (double input_sample_rate : kTestRates) {
    for (double output_sample_rate : kTestRates) {
      if (input_sample_rate == output_sample_rate) {
        continue;
      }

      SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                   input_sample_rate, output_sample_rate));
      QResampler<float> resampler(input_sample_rate, output_sample_rate);
      ASSERT_TRUE(resampler.Valid());

      const int max_excess_output =
          (resampler.factor_denominator() - 1) / resampler.factor_numerator();

      // Pad beginning of the stream with `pad` frames, 0 <= pad < denominator,
      // to test size formulas for every possible phase.
      for (int pad = 0; pad < resampler.factor_denominator(); ++pad) {
        SCOPED_TRACE(absl::StrFormat("pad: %d", pad));

        resampler.Reset();
        resampler.ProcessSamples(Eigen::ArrayXf::Zero(pad), &output);

        for (int requested = 0; requested <= 30; ++requested) {
          // How much input is needed to produce at least `requested` frames?
          int num_input = resampler.NextNumInputFramesToProduce(requested);
          // Check how much output is actually produced from num_input frames.
          // It may exceed `requested` by up to max_excess_output.
          int actual_output = resampler.NextNumOutputFrames(num_input);
          ASSERT_LE(requested, actual_output);
          ASSERT_LE(actual_output, requested + max_excess_output);
          // Always have NextNumInputFramesToProduce <= MaxInputFramesToProduce.
          ASSERT_LE(num_input, resampler.MaxInputFramesToProduce(requested));
          if (num_input > 1) {
            // Check that any fewer than num_input frames is not enough.
            ASSERT_GT(requested, resampler.NextNumOutputFrames(num_input - 1));
          }
        }

        for (int num_input = 0; num_input <= 30; ++num_input) {
          // In the reverse direction, the following should also hold.
          int num_output = resampler.NextNumOutputFrames(num_input);
          ASSERT_LE(resampler.NextNumInputFramesToProduce(num_output),
                    num_input);
        }
      }
    }
  }
}

template <typename ResamplerType>
class ResamplerQTypedTest : public ::testing::Test {};

using TestResamplerTypes =
    ::testing::Types<float, double, std::complex<float>, std::complex<double>>;
TYPED_TEST_SUITE(ResamplerQTypedTest, TestResamplerTypes);

// Reference resampling implementation.
template <typename EigenType>
std::vector<typename EigenType::Scalar> ReferenceResampling(
    const ::RationalFactorResamplerKernel* kernel, double rational_factor,
    const EigenType& input) {
  const int num_channels = input.rows();
  const int num_input_frames = input.cols();
  using ValueType = typename EigenType::Scalar;
  std::vector<ValueType> output;

  for (int m = 0;; ++m) {
    const double n0 = m * rational_factor;
    // Determine the range of n values for `sum_n x[n] h(m F/F' - n)`.
    const int n_first = static_cast<int>(std::round(n0 - kernel->radius));
    const int n_last = static_cast<int>(std::round(n0 + kernel->radius));
    // The kernel `h(m F/F' - n)` is zero outside of [n_first, n_last].
    ABSL_CHECK_EQ(::RationalFactorResamplerKernelEval(kernel, n0 - (n_first - 1)),
             0.0);
    ABSL_CHECK_EQ(::RationalFactorResamplerKernelEval(kernel, n0 - (n_last + 1)),
             0.0);

    if (n_last >= num_input_frames) {
      break;
    }

    for (int c = 0; c < num_channels; ++c) {
      // Compute `sum_n x[n] h(m F/F' - n)`.
      using SumType =
          typename std::conditional<std::is_pod<ValueType>::value, double,
                                    std::complex<double>>::type;
      SumType sum = 0.0;
      for (int n = n_first; n <= n_last; ++n) {
        sum += (n < 0 ? SumType(0.0) : static_cast<SumType>(input(c, n))) *
               ::RationalFactorResamplerKernelEval(kernel, n0 - n);
      }
      output.push_back(static_cast<ValueType>(sum));
    }
  }

  return output;
}

// Compare QResampler with ReferenceResampling().
TYPED_TEST(ResamplerQTypedTest, CompareWithReferenceResampler) {
  using ValueType = TypeParam;
  using Buffer = Eigen::Array<ValueType, Eigen::Dynamic, Eigen::Dynamic>;
  std::srand(0);
  constexpr int kNumInputFrames = 50;
  QResampler<ValueType> resampler;
  EXPECT_FALSE(resampler.Valid());

  for (int num_channels : {1, 3}) {
    SCOPED_TRACE(absl::StrFormat("num_channels: %d", num_channels));

    for (double input_sample_rate : kTestRates) {
      for (double output_sample_rate : kTestRates) {
        if (input_sample_rate == output_sample_rate) {
          continue;
        }
        SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                     input_sample_rate, output_sample_rate));
        for (double radius_factor : {4, 5, 17}) {
          SCOPED_TRACE(absl::StrFormat("radius_factor: %g", radius_factor));

          QResamplerParams params;
          params.filter_radius_factor = radius_factor;
          ::RationalFactorResamplerKernel kernel;
          ASSERT_TRUE(::RationalFactorResamplerKernelInit(
              &kernel, input_sample_rate, output_sample_rate,
              /*filter_radius_factor=*/params.filter_radius_factor,
              /*cutoff_proportion=*/params.cutoff_proportion,
              /*kaiser_beta=*/params.kaiser_beta));

          resampler.Init(input_sample_rate, output_sample_rate, num_channels,
                         params);
          ASSERT_TRUE(resampler.Valid());

          EXPECT_EQ(resampler.flush_frames(),
                    2 * static_cast<int>(std::ceil(kernel.radius)));
          const int factor_numerator = resampler.factor_numerator();
          const int factor_denominator = resampler.factor_denominator();
          const double rational_factor =
              static_cast<double>(factor_numerator) / factor_denominator;
          EXPECT_LE(std::abs(rational_factor - kernel.factor), 5e-4);

          // Create input of kNumInputFrames random frames followed by
          // flush_frames() frames of zeros for flushing.
          Buffer input = Buffer::Zero(
              num_channels, kNumInputFrames + resampler.flush_frames());
          input.leftCols(kNumInputFrames) =
              Buffer::Random(num_channels, kNumInputFrames);

          // Compute size of the output, including flushing.
          const int total_output_frames = resampler.NextNumOutputFrames(
              kNumInputFrames + resampler.flush_frames());
          Buffer output(num_channels, total_output_frames);

          // Call ProcessSamples() on the first kNumInputFrames, using .leftCols
          // to pass Array block expressions without copying.
          const int process_samples_output_frames =
              resampler.NextNumOutputFrames(kNumInputFrames);
          resampler.ProcessSamples(
              input.leftCols(kNumInputFrames),
              output.leftCols(process_samples_output_frames));

          // Call Flush(), using .rightCols to append to the previous output.
          const int flush_output_frames =
              resampler.NextNumOutputFrames(resampler.flush_frames());
          ASSERT_EQ(process_samples_output_frames + flush_output_frames,
                    total_output_frames);
          resampler.Flush(output.rightCols(flush_output_frames));

          // Run the reference resampling implementation for comparison.
          std::vector<ValueType> expected_samples =
              ReferenceResampling(&kernel, rational_factor, input);
          Eigen::Map<const Buffer> expected(
              expected_samples.data(), num_channels,
              expected_samples.size() / num_channels);
          // Allow output length to differ by up to two frames.
          ASSERT_NEAR(output.cols(), expected.cols(), 2);
          const int num_frames = std::min(output.cols(), expected.cols());
          ASSERT_THAT(output.leftCols(num_frames),
                      EigenArrayNear(expected.leftCols(num_frames), 5e-7));
        }
      }
    }
  }
}

// Resampling a sine wave should produce again a sine wave.
TEST(ResamplerQTest, ResampleSineWave) {
  constexpr double kFrequency = 1100.7;

  for (double input_sample_rate : kTestRates) {
    for (double output_sample_rate : kTestRates) {
      if (input_sample_rate == output_sample_rate) {
        continue;
      }

      SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                   input_sample_rate, output_sample_rate));
      std::vector<float> input;
      ComputeSineWaveVector(kFrequency, input_sample_rate, 0.0, 100, &input);

      QResampler<float> resampler(input_sample_rate, output_sample_rate);
      std::vector<float> output = QResampleSignal<float>(
          input_sample_rate, output_sample_rate, 1, {}, input);

      const double expected_duration =
          (input.size() + resampler.radius()) / input_sample_rate;
      const double actual_duration = output.size() / output_sample_rate;
      EXPECT_NEAR(actual_duration, expected_duration, 2.0 / output_sample_rate);

      // We ignore the first and last few output samples because they are
      // extrapolated as zeros.
      const int kRadius =
          (resampler.radius() * output_sample_rate) / input_sample_rate + 1;
      ASSERT_GT(output.size(), 3 * kRadius);

      output.erase(output.begin() + output.size() - 2 * kRadius, output.end());
      std::vector<float> expected;
      ComputeSineWaveVector(kFrequency, output_sample_rate, 0.0, output.size(),
                            &expected);
      expected.erase(expected.begin(), expected.begin() + kRadius);
      output.erase(output.begin(), output.begin() + kRadius);
      EXPECT_THAT(output, FloatArrayNear(expected, 0.1));
    }
  }
}

// Test streaming with blocks of random sizes between 0 and kMaxBlockSize.
TYPED_TEST(ResamplerQTypedTest, StreamingRandomBlockSizes) {
  using ValueType = TypeParam;
  using Buffer = Eigen::Array<ValueType, Eigen::Dynamic, Eigen::Dynamic>;
  std::srand(0);
  std::mt19937 rng(0 /* seed */);
  const int kMaxBlockSize = 16;
  std::uniform_int_distribution<int> block_size_dist(0, kMaxBlockSize);
  QResampler<ValueType> resampler;

  for (int num_channels : {1, 3}) {
    SCOPED_TRACE(absl::StrFormat("num_channels: %d", num_channels));
    Buffer input = Buffer::Random(num_channels, 400);  // 400 input frames.

    for (double input_sample_rate : kTestRates) {
      for (double output_sample_rate : kTestRates) {
        if (input_sample_rate == output_sample_rate) {
          continue;
        }

        SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                     input_sample_rate, output_sample_rate));
        resampler.Init(input_sample_rate, output_sample_rate, num_channels);
        ASSERT_TRUE(resampler.Valid());

        Buffer nonstreaming;
        const int total_output = resampler.NextNumOutputFrames(input.cols());
        resampler.ProcessSamples(input, &nonstreaming);
        ASSERT_EQ(nonstreaming.cols(), total_output);

        resampler.Reset();
        Buffer streaming(num_channels, nonstreaming.cols());
        const int max_output = resampler.MaxOutputFrames(kMaxBlockSize);
        int output_written = 0;

        // Streaming processing in blocks of 0 to 16 frames at a time.
        for (int start = 0; start < input.cols();) {
          int current_block_size =
              std::min<int>(block_size_dist(rng), input.cols() - start);
          const int current_output_size =
              resampler.NextNumOutputFrames(current_block_size);
          ASSERT_LE(current_output_size, max_output);
          ASSERT_LE(output_written + current_output_size, total_output);
          resampler.ProcessSamples(
              input.middleCols(start, current_block_size),
              streaming.middleCols(output_written, current_output_size));

          start += current_block_size;
          output_written += current_output_size;
        }

        ASSERT_EQ(output_written, total_output);
        ASSERT_THAT(streaming, EigenArrayNear(nonstreaming, 5e-7));
      }
    }
  }
}

TYPED_TEST(ResamplerQTypedTest, ArgTypes) {
  using ValueType = TypeParam;
  using Buffer = Eigen::Array<ValueType, Eigen::Dynamic, Eigen::Dynamic>;
  std::srand(0);
  constexpr int kNumInputFrames = 50;
  QResampler<ValueType> resampler;

  for (int num_channels : {1, 3}) {
    SCOPED_TRACE(absl::StrFormat("num_channels: %d", num_channels));
    Buffer input = Buffer::Random(num_channels, kNumInputFrames);

    for (double input_sample_rate : kTestRates) {
      for (double output_sample_rate : kTestRates) {
        if (input_sample_rate == output_sample_rate) {
          continue;
        }

        SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                     input_sample_rate, output_sample_rate));

        resampler.Init(input_sample_rate, output_sample_rate, num_channels);
        ASSERT_TRUE(resampler.Valid());
        const int num_output_frames =
            resampler.NextNumOutputFrames(kNumInputFrames);

        // In: vector<T>, Out: vector<T>*.
        std::vector<ValueType> expected;
        resampler.ProcessSamples(input, &expected);

        // In: Span<const T>, Out: vector<T>*.
        {
          resampler.Reset();
          std::vector<ValueType> output;
          Eigen::internal::set_is_malloc_allowed(false);
          resampler.ProcessSamples(absl::MakeConstSpan(input), &output);
          Eigen::internal::set_is_malloc_allowed(true);
          EXPECT_THAT(output, FloatArrayNear(expected, 5e-7));
        }
        // In: Span<const T>, Out: Span<T>.
        {
          resampler.Reset();
          std::vector<ValueType> output(num_channels * num_output_frames);
          absl::Span<ValueType> out_span(output);
          Eigen::internal::set_is_malloc_allowed(false);
          resampler.ProcessSamples(absl::MakeConstSpan(input), out_span);
          Eigen::internal::set_is_malloc_allowed(true);
          EXPECT_THAT(output, FloatArrayNear(expected, 5e-7));
        }
        // In: Array, Out: Array*.
        using ArrayXX = Eigen::Array<ValueType, Eigen::Dynamic, Eigen::Dynamic>;
        {
          resampler.Reset();
          ArrayXX in_array = Eigen::Map<const ArrayXX>(
              input.data(), num_channels, kNumInputFrames);
          ArrayXX out_array(num_channels, num_output_frames);
          Eigen::internal::set_is_malloc_allowed(false);
          resampler.ProcessSamples(in_array, &out_array);
          Eigen::internal::set_is_malloc_allowed(true);

          std::vector<ValueType> output(num_channels * num_output_frames);
          Eigen::Map<ArrayXX>(output.data(), num_channels, num_output_frames) =
              out_array;
          EXPECT_THAT(output, FloatArrayNear(expected, 5e-7));
        }
        // In: Block<Array>, Out: Block<Array>.
        {
          resampler.Reset();
          ArrayXX in_array(num_channels, kNumInputFrames + 10);
          in_array.leftCols(kNumInputFrames) = Eigen::Map<const ArrayXX>(
              input.data(), num_channels, kNumInputFrames);
          ArrayXX out_array(num_channels, num_output_frames + 7);
          Eigen::Block<ArrayXX, Eigen::Dynamic, Eigen::Dynamic, true>
              out_block = out_array.leftCols(num_output_frames);
          Eigen::internal::set_is_malloc_allowed(false);
          resampler.ProcessSamples(in_array.leftCols(kNumInputFrames),
                                   out_block);
          Eigen::internal::set_is_malloc_allowed(true);

          std::vector<ValueType> output(num_channels * num_output_frames);
          Eigen::Map<ArrayXX>(output.data(), num_channels, num_output_frames) =
              out_block;
          EXPECT_THAT(output, FloatArrayNear(expected, 5e-7));
        }

        // (Span<const T>, vector<T>*) also works through base class pointer.
        {
          Resampler<ValueType>* base_ptr = &resampler;
          base_ptr->Reset();
          std::vector<ValueType> output;
          base_ptr->ProcessSamples(absl::MakeConstSpan(input), &output);
          EXPECT_THAT(output, FloatArrayNear(expected, 5e-7));
        }
      }
    }
  }
}

TYPED_TEST(ResamplerQTypedTest, TestInvalidResampler) {
  QResampler<TypeParam> resampler(0, 0);
  ASSERT_FALSE(resampler.Valid());
}


// Mock resampler implementation for testing how QResampler plumbs args to
// ProcessSamplesGeneric().
struct MockResamplerImpl {
  using ValueType = float;

  template <typename DelayedInput, typename Input, typename Output>
  static void ProcessSamplesGeneric(
      const qresampler_internal::QResamplerFilters<float>& filters,
      DelayedInput&& delayed_input, int& delayed_input_frames, int& phase,
      Input&& input, Output&& output) {
    const int num_channels = delayed_input.rows();
    LOG(INFO) << "ProcessSamplesGeneric called for num_channels = "
              << num_channels;
    EXPECT_EQ(input.rows(), num_channels);
    EXPECT_EQ(output.rows(), num_channels);
    EXPECT_EQ(output.cols(), *expected_num_output_frames);

    EXPECT_EQ(absl::decay_t<DelayedInput>::RowsAtCompileTime,
              expected_rows_at_compile_time);
    EXPECT_EQ(absl::decay_t<Input>::RowsAtCompileTime,
              expected_rows_at_compile_time);
    EXPECT_EQ(absl::decay_t<Output>::RowsAtCompileTime,
              expected_rows_at_compile_time);
  }

  static int* expected_num_output_frames;
  static int expected_rows_at_compile_time;
};
int* MockResamplerImpl::expected_num_output_frames;
int MockResamplerImpl::expected_rows_at_compile_time;

// Test passing absl::Spans and std::vectors to ProcessSamples and Flush() into
// MockResamplerImpl.
TEST(ResamplerQTest, MockResamplerImpl) {
  constexpr int kNumInputFrames = 15;
  QResampler<MockResamplerImpl> mock;

  for (double input_sample_rate : kTestRates) {
    for (double output_sample_rate : kTestRates) {
      if (input_sample_rate == output_sample_rate) {
        continue;
      }
      SCOPED_TRACE(absl::StrFormat("Resampling from %gHz to %gHz",
                                   input_sample_rate, output_sample_rate));

      for (int num_channels : {1, 3}) {
        SCOPED_TRACE(absl::StrFormat("num_channels: %d", num_channels));

        ASSERT_TRUE(
            mock.Init(input_sample_rate, output_sample_rate, num_channels));
        int num_output_frames = mock.NextNumOutputFrames(kNumInputFrames);
        MockResamplerImpl::expected_num_output_frames = &num_output_frames;
        // If num_channels = 1, the specialization for single channel applies
        // and ProcessSamplesGenericImpl() should receive objects having 1 row
        // at compile time. Otherwise, the number of rows is dynamic.
        MockResamplerImpl::expected_rows_at_compile_time =
            (num_channels == 1) ? 1 : Eigen::Dynamic;

        // Test std::vector and absl::Span args.
        std::vector<float> input_vector(num_channels * kNumInputFrames);
        std::vector<float> output_vector;
        absl::Span<const float> input_span(absl::MakeConstSpan(input_vector));
        std::vector<float> output_buffer(num_channels * num_output_frames);
        absl::Span<float> output_span(absl::MakeSpan(output_buffer));
        {
          SCOPED_TRACE("ProcessSamples(const std::vector&, std::vector*)");
          mock.ProcessSamples(input_vector, &output_vector);
        }
        {
          SCOPED_TRACE("ProcessSamples(absl::Span, std::vector*)");
          mock.ProcessSamples(input_span, &output_vector);
        }
        {
          SCOPED_TRACE("ProcessSamples(const std::vector&, absl::Span)");
          mock.ProcessSamples(input_vector, output_span);
        }
        {
          SCOPED_TRACE("ProcessSamples(absl::Span, absl::Span)");
          mock.ProcessSamples(input_span, output_span);
        }

        num_output_frames = mock.NextNumOutputFrames(mock.flush_frames());
        {
          SCOPED_TRACE("Flush(std::vector*)");
          mock.Flush(&output_vector);
        }
        {
          std::vector<float> flush_output_buffer(
              num_channels * num_output_frames);
          absl::Span<float> flush_output_span(
              absl::MakeSpan(flush_output_buffer));
          SCOPED_TRACE("Flush(absl::Span)");
          mock.Flush(flush_output_span);
        }

        // Test Eigen args.
        num_output_frames = mock.NextNumOutputFrames(kNumInputFrames);
        Eigen::ArrayXXf input_array(num_channels, kNumInputFrames);
        Eigen::ArrayXXf flush_output_array;
        {
          SCOPED_TRACE(
              "ProcessSamples(const Eigen::ArrayXXf&, Eigen::MatrixXf*)");
          Eigen::MatrixXf output_matrix;
          mock.ProcessSamples(input_array, &output_matrix);
        }
        {
          SCOPED_TRACE(
              "ProcessSamples(Eigen::Block<ArrayXXf>, Eigen::Block<ArrayXXf>)");
          Eigen::ArrayXXf output_array(num_channels, num_output_frames + 1);
          Eigen::internal::set_is_malloc_allowed(false);
          mock.ProcessSamples(input_array.leftCols(kNumInputFrames),
                              output_array.leftCols(num_output_frames));
          Eigen::internal::set_is_malloc_allowed(true);
        }

        num_output_frames = mock.NextNumOutputFrames(mock.flush_frames());
        {
          SCOPED_TRACE("Flush(Eigen::ArrayXXf*)");
          mock.Flush(&flush_output_array);
        }
        {
          SCOPED_TRACE("Flush(Eigen::Block<ArrayXXf>)");
          Eigen::internal::set_is_malloc_allowed(false);
          mock.Flush(flush_output_array.leftCols(num_output_frames));
          Eigen::internal::set_is_malloc_allowed(true);
        }
      }

      // Test with column vector args. This checks that TransposeToRowVector()
      // is applied to the input and output.
      {
        SCOPED_TRACE("num_channels: 1");
        ASSERT_TRUE(mock.Init(input_sample_rate, output_sample_rate));
        int num_output_frames = mock.NextNumOutputFrames(kNumInputFrames);
        MockResamplerImpl::expected_num_output_frames = &num_output_frames;
        MockResamplerImpl::expected_rows_at_compile_time = 1;
        Eigen::ArrayXf input_array(kNumInputFrames);

        {
          SCOPED_TRACE(
              "ProcessSamples(const Eigen::ArrayXf&, Eigen::VectorXf*)");
          Eigen::VectorXf output_vector(1);
          mock.ProcessSamples(input_array, &output_vector);
        }
        {
          SCOPED_TRACE(
              "ProcessSamples(Eigen::VectorBlock<ArrayXf>, "
              "Eigen::VectorBlock<ArrayXf>)");
          Eigen::ArrayXf output_array(num_output_frames + 1);
          Eigen::internal::set_is_malloc_allowed(false);
          mock.ProcessSamples(input_array.head(kNumInputFrames),
                              output_array.head(num_output_frames));
          Eigen::internal::set_is_malloc_allowed(true);
        }
      }

      // Test that number of rows is statically inferred when possible.
      {
        constexpr int kNumChannels = 3;
        SCOPED_TRACE(absl::StrFormat("kNumChannels: %d", kNumChannels));
        ASSERT_TRUE(
            mock.Init(input_sample_rate, output_sample_rate, kNumChannels));
        int num_output_frames = mock.NextNumOutputFrames(kNumInputFrames);
        MockResamplerImpl::expected_num_output_frames = &num_output_frames;
        MockResamplerImpl::expected_rows_at_compile_time = kNumChannels;

        {
          SCOPED_TRACE(
              "ProcessSamples(const Eigen::Array<float, kNumChannels, "
              "Dynamic>&, Eigen::ArrayXXf*)");
          Eigen::Array<float, kNumChannels, Eigen::Dynamic>
              input_array_fixed_rows(kNumChannels, kNumInputFrames);
          Eigen::ArrayXXf output_array;
          mock.ProcessSamples(input_array_fixed_rows, &output_array);
        }
        {
          SCOPED_TRACE(
              "ProcessSamples(Eigen::CwiseNullaryOp, Eigen::Matrix<float, "
              "kNumChannels, Dynamic>*)");
          Eigen::Matrix<float, kNumChannels, Eigen::Dynamic>
              output_matrix_fixed_rows;
          mock.ProcessSamples(Eigen::MatrixXf::Random(kNumChannels, 15),
                              &output_matrix_fixed_rows);
        }
      }
    }
  }
}

}  // namespace
}  // namespace audio_dsp

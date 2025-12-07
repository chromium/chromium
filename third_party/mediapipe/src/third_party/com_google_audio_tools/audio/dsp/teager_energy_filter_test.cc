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

#include "audio/dsp/teager_energy_filter.h"

#include <cmath>
#include <random>

#include "audio/dsp/signal_vector_util.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace audio_dsp {
namespace {

constexpr float kSampleRateHz = 8000.0f;
constexpr float kCenterHz = 900.0f;
constexpr float kNyquistHz = kSampleRateHz / 2;
constexpr int kNumSamples = 50;

TEST(TeagerEnergyFilter, DeathTest) {
  // Test that invalid sample and center frequencies cause construction to
  // fail a ABSL_CHECK.

  // This construction should succeed.
  TeagerEnergyFilter test_filter(kSampleRateHz, kCenterHz);

  EXPECT_DEATH(TeagerEnergyFilter(-kSampleRateHz, kCenterHz),
               "The sample rate must be positive.");
  EXPECT_DEATH(TeagerEnergyFilter(kSampleRateHz, 0.0),
               "The center frequency must be positive.");
  EXPECT_DEATH(TeagerEnergyFilter(kSampleRateHz, kNyquistHz),
               "The center frequency must be less than the Nyquist frequency.");
}

TEST(TeagerEnergyFilter, EnvelopeTest) {
  // Test that Teager energy approximates the signal envelope.
  int num_samples = 200;
  for (float sample_rate_hz : {8000.0f, 12000.0f}) {
    std::vector<float> timestamps(num_samples);
    std::iota(timestamps.begin(), timestamps.end(), 0.0);
    for (float& timestamp : timestamps) timestamp /= sample_rate_hz;

    for (float frequency_hz : {90.0f, 900.0f, 3800.0f}) {
      for (float amplitude : {0.314f, 0.707f}) {
        // Make a sinusoid with exponential envelope.
        auto exact_envelope = [amplitude](float timestamp) -> float {
          return amplitude * std::exp(-timestamp / 0.005); };
        std::vector<float> samples(num_samples);
        for (int i = 0; i < num_samples; ++i) {
          samples[i] = exact_envelope(timestamps[i]) *
              std::sin(2.0 * M_PI * frequency_hz * timestamps[i]);
        }

        TeagerEnergyFilter test_filter(sample_rate_hz, frequency_hz);
        std::vector<float> teager_energy(num_samples);
        test_filter.ProcessBlock(samples, &teager_energy);

        std::vector<float> envelope(num_samples);
        for (int i = 0; i < num_samples; ++i) {
          envelope[i] = std::sqrt(std::abs(teager_energy[i]));
        }

        // Compare to the exact envelope, ignoring the first two samples.
        for (int i = 2; i < num_samples; ++i) {
          EXPECT_NEAR(envelope[i],
                      exact_envelope(timestamps[i] - test_filter.Delay()),
                      1e-4);
        }
      }
    }
  }
}

TEST(TeagerEnergyFilter, StreamingTest) {
  // Test streaming with varying block sizes.
  TeagerEnergyFilter test_filter(kSampleRateHz, kCenterHz);

  // Create 50 random samples with a normal distribution.
  std::vector<float> input_samples;
  std::default_random_engine generator;
  std::normal_distribution<float> distribution;
  for (int i = 0; i < kNumSamples; ++i) {
      input_samples.emplace_back(distribution(generator));
  }

  std::vector<float> nonstreaming_result;
  test_filter.ProcessBlock(input_samples, &nonstreaming_result);
  bool (*float_isfinite)(float) = +[](float x) { return std::isfinite(x); };
  EXPECT_THAT(nonstreaming_result, testing::Each(testing::Truly(
      float_isfinite)));

  test_filter.Reset();
  std::vector<float> streaming_result;
  std::vector<float> output_block;
  // Cycle through block sizes 5, 1, 0, 3, 2, 19, 5, 1, 0, 3, 2, 19, ...
  for (int start = 0; start < kNumSamples;) {
    for (int block_size : {5, 1, 0, 3, 2, 19}) {
      block_size = std::min(block_size, kNumSamples - start);
      test_filter.ProcessBlock(std::vector<float>(
          input_samples.cbegin() + start,
          input_samples.cbegin() + start + block_size), &output_block);
      EXPECT_EQ(block_size, output_block.size());
      VectorAppend(&streaming_result, output_block);
      start += block_size;
    }
  }

  EXPECT_THAT(streaming_result, testing::ElementsAreArray(nonstreaming_result));
}

}  // namespace
}  // namespace audio_dsp

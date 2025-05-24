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

#include "audio/linear_filters/two_tap_fir_filter.h"

#include "audio/dsp/testing_util.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {
namespace {
using Eigen::ArrayXXf;

TEST(TwoTapFirTest, TwoTap) {
  TwoTapFirFilter filter({-3, 2});
  filter.Init(4);
  {
    ArrayXXf data(4, 6);
    data << 0, 1, 2, 3, 4, 5,
            1, 2, 3, 4, 5, 6,
            4, 3, 2, 1, 0, -1,
            6, 5, 4, 3, 2, 1;
    filter.ProcessBlock(data, &data);
    ArrayXXf expected(4, 6);
    expected << 0,  2,  1,  0, -1, -2,
                2,  1,  0, -1, -2, -3,
                8, -6, -5, -4, -3, -2,
               12, -8, -7, -6, -5, -4;
    EXPECT_THAT(data, audio_dsp::EigenArrayNear(expected, 1e-10));
  }
  {
    ArrayXXf data(4, 3);
    data << 0, 1, 2,
            1, 2, 3,
            4, 3, 2,
            6, 5, 4;
    filter.ProcessBlock(data, &data);
    ArrayXXf expected(4, 3);
    expected << -15,  2,  1,
                -16,  1,  0,
                 11, -6, -5,
                  9, -8, -7;
    EXPECT_THAT(data, audio_dsp::EigenArrayNear(expected, 1e-10));
  }
}

TEST(TwoTapFirTest, FirstDifference4) {
  FirstDifferenceFilter filter;
  filter.Init(4);
  {
    ArrayXXf data(4, 6);
    data << 0, 1, 2, 3, 4, 5,
            1, 2, 3, 4, 5, 6,
            4, 3, 2, 1, 0, -1,
            6, 5, 4, 3, 2, 1;
    filter.ProcessBlock(data, &data);
    ArrayXXf expected(4, 6);
    expected << 0, 1, 1, 1, 1, 1,
                1, 1, 1, 1, 1, 1,
                4, -1, -1, -1, -1, -1,
                6, -1, -1, -1, -1, -1;
    EXPECT_THAT(data, audio_dsp::EigenArrayNear(expected, 1e-10));
  }
  {
    ArrayXXf data(4, 3);
    data << 0, 1, 2,
            1, 2, 3,
            4, 3, 2,
            6, 5, 4;
    filter.ProcessBlock(data, &data);
    ArrayXXf expected(4, 3);
    expected << -5, 1, 1,
                -5, 1, 1,
               5, -1, -1,
               5, -1, -1;
    EXPECT_THAT(data, audio_dsp::EigenArrayNear(expected, 1e-10));
  }
}

TEST(TwoTapFirTest, FirstDifference6) {
  FirstDifferenceFilter filter;
  filter.Init(6);
  {
    ArrayXXf data(6, 6);
    data << 0, 1, 2, 3, 4, 5,
            1, 2, 3, 4, 5, 6,
            4, 3, 2, 1, 0, -1,
            1, 2, 3, 4, 5, 6,
            4, 3, 2, 1, 0, -1,
            6, 5, 4, 3, 2, 1;
    filter.ProcessBlock(data, &data);
    ArrayXXf expected(6, 6);
    expected << 0,  1,  1,  1,  1,  1,
                1,  1,  1,  1,  1,  1,
                4, -1, -1, -1, -1, -1,
                1,  1,  1,  1,  1,  1,
                4, -1, -1, -1, -1, -1,
                6, -1, -1, -1, -1, -1;
    EXPECT_THAT(data, audio_dsp::EigenArrayNear(expected, 1e-10));
  }
  {
    ArrayXXf data(6, 3);
    data << 0, 1, 2,
            1, 2, 3,
            4, 3, 2,
            1, 2, 3,
            4, 3, 2,
            6, 5, 4;
    filter.ProcessBlock(data, &data);
    ArrayXXf expected(6, 3);
    expected << -5,  1,  1,
                -5,  1,  1,
                 5, -1, -1,
                -5,  1,  1,
                 5, -1, -1,
                 5, -1, -1;
    EXPECT_THAT(data, audio_dsp::EigenArrayNear(expected, 1e-10));
    // Make sure a single sample doesn't break it.
    ArrayXXf single_sample_data(6, 1);
    single_sample_data << 1,
                          1,
                          1,
                          1,
                          1,
                          1;
    filter.ProcessBlock(single_sample_data, &single_sample_data);
    ArrayXXf single_sample_expected(6, 1);
    single_sample_expected << -1,
                              -2,
                              -1,
                              -2,
                              -1,
                              -3;
    EXPECT_THAT(single_sample_data,
                audio_dsp::EigenArrayNear(single_sample_expected, 1e-10));
  }
}

TEST(TwoTapFirTest, NoSamplesTest) {
  FirstDifferenceFilter filter;
  filter.Init(4);

  ArrayXXf data(4, 0);
  filter.ProcessBlock(data, &data);
  EXPECT_EQ(data.cols(), 0);
}

// Run on lpac14 (32 X 2600 MHz CPUs); 2017-03-06T17:07:58.242651049-08:00
// CPU: Intel Sandybridge with HyperThreading (16 cores)
// Benchmark             Time(ns)     CPU(ns)     Iterations
// ---------------------------------------------------------
// BM_TwoTap/1                137         137       50794798  3.487G items/s
// BM_TwoTap/2                276         276       25285455  1.728G items/s
// BM_TwoTap/3                356         356       19661932  1.340G items/s
// BM_TwoTap/4                499         499       14012315  978.812M items/s
// BM_TwoTap/5                590         589       10000000  828.480M items/s
// BM_TwoTap/6                872         871        8056236  560.506M items/s
// BM_TwoTap/7                818         816        8610291  598.048M items/s
// BM_TwoTap/8                928         926        7577594  527.086M items/s
// BM_TwoTap/9               1292        1290        5438087  378.527M items/s
// BM_TwoTap/10              1464        1462        4794215  333.904M items/s

void BM_TwoTap(benchmark::State& state) {
  const int kNumChannels = state.range(0);
  constexpr int kSamplePerBlock = 512;
  srand(0 /* seed */);
  ArrayXXf input = ArrayXXf::Random(kNumChannels, kSamplePerBlock);
  TwoTapFirFilter filter({3.2, -1.3});
  filter.Init(kNumChannels);

  while (state.KeepRunning()) {
    filter.ProcessBlock(input, &input);
    benchmark::DoNotOptimize(input);
  }
  state.SetItemsProcessed(kSamplePerBlock * state.iterations());
}
BENCHMARK(BM_TwoTap)->DenseRange(1, 10);

}  // namespace
}  // namespace linear_filters

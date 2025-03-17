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

#include "audio/dsp/fixed_delay_line.h"

#include <random>

#include "audio/dsp/testing_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace {

using audio_dsp::EigenArrayNear;

TEST(FixedDelayLineTest, DelaysMonoSignal) {
  FixedDelayLine delay_line;
  delay_line.Init(1, 4, 4);

  Eigen::ArrayXXf data(1, 4);
  data << 1, 2, 3, 4;

  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(Eigen::ArrayXXf::Zero(1, 4), 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(data, 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(data, 1e-5));
}

TEST(FixedDelayLineTest, DelaysMonoSignalSmallerBuffers) {
  FixedDelayLine delay_line;
  delay_line.Init(1, 4, 2);

  Eigen::ArrayXXf data(1, 2);
  data << 1, 2;

  // Two buffers of nothing.
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(Eigen::ArrayXXf::Zero(1, 2), 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(Eigen::ArrayXXf::Zero(1, 2), 1e-5));
  // Start to get the data back.
  EXPECT_THAT(delay_line.ProcessBlock(data), EigenArrayNear(data, 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data), EigenArrayNear(data, 1e-5));
}

TEST(FixedDelayLineTest, DelaysMonoSignalLargerBuffers) {
  FixedDelayLine delay_line;
  delay_line.Init(1, 4, 6);

  Eigen::ArrayXXf data(1, 6);
  data << 1, 2, 3, 4, 5, 6;

  Eigen::ArrayXXf first_expected(1, 6);
  first_expected << 0, 0, 0, 0, 1, 2;
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(first_expected, 1e-5));

  Eigen::ArrayXXf repeated_expected(1, 6);
  repeated_expected << 3, 4, 5, 6, 1, 2;
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
}

TEST(FixedDelayLineTest, DelaysMonoSignalLargerBuffersDifferentDelay) {
  FixedDelayLine delay_line;
  delay_line.Init(1, 1, 6);

  Eigen::ArrayXXf data(1, 6);
  data << 1, 2, 3, 4, 5, 6;

  Eigen::ArrayXXf first_expected(1, 6);
  first_expected << 0, 1, 2, 3, 4, 5;
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(first_expected, 1e-5));

  Eigen::ArrayXXf repeated_expected(1, 6);
  repeated_expected << 6, 1, 2, 3, 4, 5;
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
}

TEST(FixedDelayLineTest, LargerBlockThanExpected) {
  FixedDelayLine delay_line;
  delay_line.Init(1, 2, 4);

  Eigen::ArrayXXf data(1, 6);
  data << 1, 2, 3, 4, 5, 6;

  Eigen::ArrayXXf first_expected(1, 6);
  first_expected << 0, 0, 1, 2, 3, 4;
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(first_expected, 1e-5));

  Eigen::ArrayXXf repeated_expected(1, 6);
  repeated_expected << 5, 6, 1, 2, 3, 4;
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
}

TEST(FixedDelayLineTest, MuchLargerBlockThanExpected) {
  FixedDelayLine delay_line;
  delay_line.Init(1, 2, 4);

  Eigen::ArrayXXf data(1, 12);
  data << 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12;

  Eigen::ArrayXXf first_expected(1, 12);
  first_expected << 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10;
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(first_expected, 1e-5));

  Eigen::ArrayXXf repeated_expected(1, 12);
  repeated_expected << 11, 12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10;
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
}

TEST(FixedDelayLineTest, DifferentSizedBlocks) {
  FixedDelayLine delay_line;
  delay_line.Init(1, 3, 4);

  Eigen::ArrayXXf data12(1, 12);
  data12 << 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12;

  Eigen::ArrayXXf expected1(1, 12);
  expected1 << 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9;
  EXPECT_THAT(delay_line.ProcessBlock(data12), EigenArrayNear(expected1, 1e-5));

  Eigen::ArrayXXf data4(1, 4);
  data4 << 1, 2, 3, 4;
  Eigen::ArrayXXf expected2(1, 4);
  expected2 << 10, 11, 12, 1;
  EXPECT_THAT(delay_line.ProcessBlock(data4), EigenArrayNear(expected2, 1e-5));

  Eigen::ArrayXXf data0(1, 0);
  Eigen::ArrayXXf expected3(1, 0);
  EXPECT_THAT(delay_line.ProcessBlock(data0), EigenArrayNear(expected3, 1e-5));

  Eigen::ArrayXXf data7(1, 7);
  data7 << 1, 2, 3, 4, 5, 6, 7;
  Eigen::ArrayXXf expected4(1, 7);
  expected4 << 2, 3, 4, 1, 2, 3, 4;
  EXPECT_THAT(delay_line.ProcessBlock(data7), EigenArrayNear(expected4, 1e-5));

  Eigen::ArrayXXf data2(1, 2);
  data2 << 1, 2;
  Eigen::ArrayXXf expected5(1, 2);
  expected5 << 5, 6;
  EXPECT_THAT(delay_line.ProcessBlock(data2), EigenArrayNear(expected5, 1e-5));
}

TEST(FixedDelayLineTest, RandomBlockSizes) {
  FixedDelayLine delay_line;
  constexpr int kChannels = 3;
  constexpr int kNumSamples = 1000;
  for (int delay_samples : {0, 3, 9}) {
    delay_line.Init(kChannels, delay_samples, 0);

    Eigen::ArrayXXf all_data =
        Eigen::ArrayXXf::Random(kChannels, kNumSamples);
    Eigen::ArrayXXf actual =
        Eigen::ArrayXXf::Zero(kChannels, kNumSamples);

    const int kMaxBlockSize = 50;
    std::mt19937 rng(0 /* seed */);
    for (int i = 0; i < kNumSamples;) {
      std::uniform_int_distribution<int> block_size_distribution(
          0, std::min<int>(kMaxBlockSize, kNumSamples - i));
      const int block_size = block_size_distribution(rng);
      Eigen::Map<const Eigen::ArrayXXf> input(all_data.data() + kChannels * i,
                                              kChannels, block_size);
      Eigen::Map<Eigen::ArrayXXf> output(actual.data() + kChannels * i,
                                         kChannels, block_size);
      delay_line.ProcessBlock(input, &output);
      i += block_size;
    }

    EXPECT_THAT(actual.rightCols(kNumSamples - delay_samples),
                EigenArrayNear(all_data.leftCols(kNumSamples - delay_samples),
                               1e-5));
    EXPECT_THAT(actual.leftCols(delay_samples),
                EigenArrayNear(Eigen::ArrayXXf::Zero(kChannels, delay_samples),
                               1e-5));
  }
}

TEST(FixedDelayLineTest, ResetTest) {
  FixedDelayLine delay_line;
  delay_line.Init(1, 2, 4);

  Eigen::ArrayXXf data(1, 12);
  data << 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12;

  Eigen::ArrayXXf first_expected(1, 12);
  first_expected << 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10;
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(first_expected, 1e-5));

  Eigen::ArrayXXf repeated_expected(1, 12);
  repeated_expected << 11, 12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10;
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));

  delay_line.Reset();
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(first_expected, 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
  EXPECT_THAT(delay_line.ProcessBlock(data),
              EigenArrayNear(repeated_expected, 1e-5));
}

}  // namespace

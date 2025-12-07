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

#include "audio/linear_filters/fir_filter.h"

#include <random>

#include "audio/dsp/testing_util.h"
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

namespace linear_filters {
namespace {

using audio_dsp::EigenArrayNear;

TEST(FirFilterTest, ConovlutionWithImpulseTest) {
  FirFilter fir;
  constexpr int kOneChannel = 1;
  Eigen::ArrayXf kernel(1);
  kernel << 1;

  fir.Init(kOneChannel, kernel);

  const Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kOneChannel, 12);
  Eigen::ArrayXXf output(kOneChannel, 12);
  fir.ProcessBlock(input, &output);

  EXPECT_THAT(output, EigenArrayNear(input, 1e-5));
}

TEST(FirFilterTest, ResetTest) {
  constexpr int kTwoChannel = 2;
  Eigen::ArrayXXf kernel(2, 3);
  kernel << 1, 1, 3,
            2, 3, 5;
  FirFilter fir;
  fir.Init(kernel);
  FirFilter fir_expected;
  fir_expected.Init(kernel);

  const Eigen::ArrayXXf input = Eigen::ArrayXXf::Random(kTwoChannel, 12);
  Eigen::ArrayXXf output(kTwoChannel, 12);
  Eigen::ArrayXXf expected(kTwoChannel, 12);
  fir.ProcessBlock(input, &output);
  fir.Reset();
  fir.ProcessBlock(input, &output);
  fir_expected.ProcessBlock(input, &expected);

  EXPECT_THAT(output, EigenArrayNear(expected, 1e-5));
}

TEST(FirFilterTest, SpacedImpulseTest) {
  FirFilter fir;
  constexpr int kOneChannel = 1;
  Eigen::ArrayXf kernel(3);
  kernel << 1, 1, 1;
  fir.Init(kOneChannel, kernel);

  Eigen::ArrayXXf input(kOneChannel, 12);
  input << 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0;
  Eigen::ArrayXXf output(kOneChannel, 12);
  fir.ProcessBlock(input, &output);

  EXPECT_THAT(output,
              audio_dsp::EigenArrayEq(Eigen::ArrayXXf::Ones(kOneChannel, 12)));
}

TEST(FirFilterTest, SpacedImpulseLargerKernelTest) {
  FirFilter fir;
  constexpr int kOneChannel = 1;
  Eigen::ArrayXf kernel(4);
  kernel << 1, 1, 1, 1;
  fir.Init(kOneChannel, kernel);

  Eigen::ArrayXXf input(kOneChannel, 12);
  input << 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0;
  Eigen::ArrayXXf output(kOneChannel, 12);
  fir.ProcessBlock(input, &output);

  Eigen::ArrayXXf expected(kOneChannel, 12);
  expected << 1, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0;
  EXPECT_THAT(output, EigenArrayNear(expected, 1e-5));
}

TEST(FirFilterTest, TwoChannelFilterOneDimensionalKernelTest) {
  FirFilter fir;
  constexpr int kTwoChannel = 2;
  Eigen::ArrayXf kernel(4);
  kernel << 1, 1, 1, 1;
  fir.Init(kTwoChannel, kernel);

  Eigen::ArrayXXf input(kTwoChannel, 12);
  input << 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0,
           3, 0, 0, 3, 0, 0, 3, 0, 0, 0, 0, 0;
  Eigen::ArrayXXf output(kTwoChannel, 12);
  fir.ProcessBlock(input, &output);

  Eigen::ArrayXXf expected(kTwoChannel, 12);
  expected << 1, 1, 1, 2, 1, 1, 2, 1, 1, 1, 0, 0,
              3, 3, 3, 6, 3, 3, 6, 3, 3, 3, 0, 0;
  EXPECT_THAT(output, EigenArrayNear(expected, 1e-5));
}

TEST(FirFilterTest, TwoChannelFilterTwoDimensionalKernelTest) {
  FirFilter fir;
  constexpr int kTwoChannel = 2;
  Eigen::ArrayXXf kernel(2, 4);
  kernel << 1, 2, 3, 4,
            4, 3, 2, 1;
  fir.Init(kernel);

  Eigen::ArrayXXf input(kTwoChannel, 12);
  input << 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0,
           3, 0, 0, 3, 0, 0, 3, 0, 0, 0, 0, 0;
  Eigen::ArrayXXf output(kTwoChannel, 12);
  fir.ProcessBlock(input, &output);

  Eigen::ArrayXXf expected(kTwoChannel, 12);
  expected << 1,  2, 3,  5, 2, 3,  5, 2, 3, 4, 0, 0,
              12, 9, 6, 15, 9, 6, 15, 9, 6, 3, 0, 0;
  EXPECT_THAT(output, EigenArrayNear(expected, 1e-5));
}

TEST(FirFilterTest, SpacedImpulseLargerKernelStreamingTest) {
  FirFilter fir;
  Eigen::ArrayXf kernel(4);
  kernel << 1, 1, 1, 1;
  fir.Init(1, kernel);

  Eigen::ArrayXXf input1(1, 2);
  input1 << 1, 0;
  Eigen::ArrayXXf input2(1, 6);
  input2 << 0, 1, 0, 0, 1, 0;
  Eigen::ArrayXXf input3(1, 3);
  input3 << 0, 0, 0;
  Eigen::ArrayXXf output1;
  Eigen::ArrayXXf output2;
  Eigen::ArrayXXf output3;
  fir.ProcessBlock(input1, &output1);
  fir.ProcessBlock(input2, &output2);
  fir.ProcessBlock(input3, &output3);

  Eigen::ArrayXXf expected1(1, 2);
  expected1 << 1, 1;
  Eigen::ArrayXXf expected2(1, 6);
  expected2 << 1, 2, 1, 1, 2, 1;
  Eigen::ArrayXXf expected3(1, 3);
  expected3 << 1, 1, 0;
  EXPECT_THAT(output1, EigenArrayNear(expected1, 1e-5));
  EXPECT_THAT(output2, EigenArrayNear(expected2, 1e-5));
  EXPECT_THAT(output3, EigenArrayNear(expected3, 1e-5));
}

TEST(FirFilterTest, ScaledSpacedImpulseLargerKernelStreamingTest) {
  FirFilter fir;
  Eigen::ArrayXf kernel(4);
  kernel << 1, 2, 1, 1;
  fir.Init(1, kernel);

  Eigen::ArrayXXf input1(1, 2);
  input1 << 1, 0;
  Eigen::ArrayXXf input2(1, 6);
  input2 << 0, 2, 0, 0, 1, 0;
  Eigen::ArrayXXf input3(1, 3);
  input3 << 0, 0, 0;
  Eigen::ArrayXXf output1;
  Eigen::ArrayXXf output2;
  Eigen::ArrayXXf output3;
  fir.ProcessBlock(input1, &output1);
  fir.ProcessBlock(input2, &output2);
  fir.ProcessBlock(input3, &output3);

  Eigen::ArrayXXf expected1(1, 2);
  expected1 << 1, 2;
  Eigen::ArrayXXf expected2(1, 6);
  expected2 << 1, 3, 4, 2, 3, 2;
  Eigen::ArrayXXf expected3(1, 3);
  expected3 << 1, 1, 0;
  EXPECT_THAT(output1, EigenArrayNear(expected1, 1e-5));
  EXPECT_THAT(output2, EigenArrayNear(expected2, 1e-5));
  EXPECT_THAT(output3, EigenArrayNear(expected3, 1e-5));
}

TEST(FirFilterTest, RandomBlockSizes) {
  FirFilter fir;
  constexpr int kChannels = 3;
  constexpr int kKernelNumSamples = 25;
  constexpr int kNumSamples = 1000;
  const Eigen::ArrayXXf kernel = Eigen::ArrayXXf::Ones(kChannels,
                                                         kKernelNumSamples);
  fir.Init(kernel);

  const Eigen::ArrayXXf all_data =
      Eigen::ArrayXXf::Ones(kChannels, kNumSamples);
  Eigen::ArrayXXf actual =
      Eigen::ArrayXXf::Zero(kChannels, kNumSamples);

  const int kMaxBlockSize = kKernelNumSamples * 2;

  std::mt19937 rng(0 /* seed */);
  for (int i = 0; i < kNumSamples;) {
    std::uniform_int_distribution<int> block_size_distribution(
        0, std::min<int>(kMaxBlockSize, kNumSamples - i));
    const int block_size = block_size_distribution(rng);
    Eigen::Map<const Eigen::ArrayXXf> input(all_data.data() + kChannels * i,
                                            kChannels, block_size);
    Eigen::Map<Eigen::ArrayXXf> output(actual.data() + kChannels * i,
                                       kChannels, block_size);
    fir.ProcessBlock(input, &output);
    i += block_size;
  }

  // Process the whole block at once and make sure it's the same.
  Eigen::ArrayXXf expected =
      Eigen::ArrayXXf::Zero(kChannels, kNumSamples);
  FirFilter fir_expected;
  fir_expected.Init(kernel);
  fir_expected.ProcessBlock(all_data, &expected);
  EXPECT_THAT(actual, EigenArrayNear(expected, 1e-5));
}

}  // namespace
}  // namespace linear_filters

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

#include "audio/linear_filters/filterbanks/factor_two_decimator.h"

#include "audio/dsp/testing_util.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace linear_filters {
namespace {

using Eigen::ArrayXXf;

TEST(FactorTwoDecimatorTest, BasicTest) {
  FactorTwoDecimator decimator;
  decimator.Init(3);
  {
    ArrayXXf data(3, 6);
    data << -9, -8, -7, -6, -5, -4,
            -3, -2, -1,  0,  1,  2,
             3,  4,  5,  6,  7,  8;
    auto decimated_data = decimator.Decimate(data);
    ArrayXXf expected(3, 3);
    expected << -9, -7, -5,
                -3, -1,  1,
                 3,  5,  7;
    EXPECT_THAT(decimated_data, audio_dsp::EigenArrayNear(expected, 1e-10));
  }
  // Tests that for odd numbers of samples, the right number of samples come
  // out.
  for (int i = 0; i < 2; ++i) {
    {
      // Pass an odd-sized block and make sure we get the first and third
      // samples out of the decimator. The first sample of the next block
      // (after this one should be skipped).
      ArrayXXf data(3, 3);
      data << 0, 1, 2,
              1, 2, 3,
              4, 3, 2;
      auto decimated_data = decimator.Decimate(data);
      ArrayXXf expected(3, 2);
      expected << 0, 2,
                  1, 3,
                  4, 2;
      EXPECT_THAT(decimated_data, audio_dsp::EigenArrayNear(expected, 1e-10));
    }
    {
      // Pass another odd block size. We should only get the second sample out.
      // Since the third sample of this block is skipped, the first sample of
      // the next block (after this one) should not be skipped. This is tested
      // on the second pass through the loop.
      ArrayXXf data(3, 3);
      data << 5, 1, 6,
              6, 2, 7,
              7, 3, 7;
      auto decimated_data = decimator.Decimate(data);
      ArrayXXf expected(3, 1);
      expected << 1,
                  2,
                  3;
      EXPECT_THAT(decimated_data, audio_dsp::EigenArrayNear(expected, 1e-10));
    }
  }
}

TEST(FactorTwoDecimatorTest, NoSamplesTest) {
  FactorTwoDecimator decimator;
  decimator.Init(4);

  ArrayXXf data(4, 0);
  ArrayXXf output = decimator.Decimate(data);
  EXPECT_EQ(output.cols(), 0);
}

}  // namespace
}  // namespace linear_filters

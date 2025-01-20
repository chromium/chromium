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

#include "audio/dsp/mfcc/dct.h"

#include "gtest/gtest.h"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {

TEST(DctTest, AgreesWithMatlab) {
  // This test verifies the DCT against MATLAB's dct function.
  Dct dct;
  std::vector<double> input = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  const int kCoefficientCount = 6;
  ASSERT_TRUE(dct.Initialize(input.size(), kCoefficientCount));
  std::vector<double> output;
  dct.Compute(input, &output);
  // Note, the matlab dct function divides the first coefficient by
  // sqrt(2), whereas we don't, so we multiply the first element of
  // the matlab result by sqrt(2) to get the expected values below.
  std::vector<double> expected = {12.1243556530, -4.1625617959, 0.0,
                             -0.4082482905, 0.0,           -0.0800788912};
  ASSERT_EQ(output.size(), kCoefficientCount);
  for (int i = 0; i < kCoefficientCount; ++i) {
    EXPECT_NEAR(output[i], expected[i], 1e-10);
  }
}

TEST(DctTest, InitializeFailsOnInvalidInput) {
  Dct dct1;
  EXPECT_FALSE(dct1.Initialize(-50, 1));
  Dct dct2;
  EXPECT_FALSE(dct1.Initialize(10, -4));
  Dct dct3;
  EXPECT_FALSE(dct1.Initialize(-1, -1));
  Dct dct4;
  EXPECT_FALSE(dct1.Initialize(20, 21));
}

}  // namespace audio_dsp

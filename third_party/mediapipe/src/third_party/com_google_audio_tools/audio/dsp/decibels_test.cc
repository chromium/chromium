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

#include "audio/dsp/decibels.h"

#include "audio/dsp/testing_util.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {
namespace {

TEST(DecibelsTest, WorksForInt) {
  EXPECT_NEAR(AmplitudeRatioToDecibels(2), 6.02, 1e-2);
  for (int i = 1; i < 100; ++i) {
    ASSERT_NEAR(DecibelsToAmplitudeRatio(
        AmplitudeRatioToDecibels(i)), i, 1e-12);
  }
  for (int i = 1; i < 100; ++i) {
    ASSERT_NEAR(DecibelsToPowerRatio(
        PowerRatioToDecibels(i)), i, 1e-12);
  }
}

TEST(DecibelsTest, TestConversions) {
  EXPECT_EQ(AmplitudeRatioToDecibels(1.0), 0);
  // 6dB = 2 is a common rule of thumb. The equality is only approximate.
  EXPECT_NEAR(AmplitudeRatioToDecibels(2.0), 6, 3e-2);
  EXPECT_NEAR(AmplitudeRatioToDecibels(0.001), -60, 1e-12);

  for (double f = 0.001; f < 100; f *= 1.2) {
    ASSERT_NEAR(DecibelsToAmplitudeRatio(
        AmplitudeRatioToDecibels(f)), f, 1e-12);
  }
}

TEST(DecibelsTest, EigenTestConversions) {
  Eigen::Array3d test_points(1.0, 2.0, 0.001);
  Eigen::Array3d result;
  Eigen::Array3d expected;
  expected << 0, AmplitudeRatioToDecibels(2.0), -60;
  AmplitudeRatioToDecibels(test_points, &result);
  EXPECT_THAT(result, EigenArrayNear(expected, 1e-12));

  // Get out of the range of Eigen::ArrayXf::Random() (-1, 1).
  Eigen::ArrayXf arr = 20 * (Eigen::ArrayXf::Random(100) + 2);
  Eigen::ArrayXf arr_db;
  Eigen::ArrayXf arr_result;
  AmplitudeRatioToDecibels(arr, &arr_db);
  DecibelsToAmplitudeRatio(arr_db, &arr_result);
  EXPECT_THAT(arr, EigenArrayNear(arr_result, 1e-4));

  Eigen::MatrixXd mat = 20 * (Eigen::MatrixXd::Random(2, 100).array() + 2);
  Eigen::MatrixXd mat_db;
  Eigen::MatrixXd mat_result;
  AmplitudeRatioToDecibels(mat, &mat_db);
  DecibelsToAmplitudeRatio(mat_db, &mat_result);
  EXPECT_THAT(mat, EigenArrayNear(mat_result, 1e-12));
}

TEST(DecibelsTest, PowerTestConversions) {
  EXPECT_EQ(PowerRatioToDecibels(1.0), 0);
  EXPECT_NEAR(PowerRatioToDecibels(2.0), 3, 3e-2);
  EXPECT_NEAR(PowerRatioToDecibels(0.001), -30, 1e-12);

  for (double f = 0.001; f < 100; f *= 1.2) {
    ASSERT_NEAR(DecibelsToPowerRatio(
        PowerRatioToDecibels(f)), f, 1e-12);
  }
}

TEST(DecibelsTest, EigenPowerTestConversions) {
  Eigen::Array3d test_points;
  Eigen::Array3d result;
  test_points << 1.0, 2.0, 0.001;
  Eigen::Array3d expected;
  expected << 0, PowerRatioToDecibels(2.0), -30;
  PowerRatioToDecibels(test_points, &result);
  EXPECT_THAT(result, EigenArrayNear(expected, 1e-12));

  // Get out of the range (-1, 1).
  Eigen::ArrayXf arr = 20 * (Eigen::ArrayXf::Random(100) + 2);
  Eigen::ArrayXf arr_db;
  Eigen::ArrayXf arr_result;
  PowerRatioToDecibels(arr, &arr_db);
  DecibelsToPowerRatio(arr_db, &arr_result);
  EXPECT_THAT(arr, EigenArrayNear(arr_result, 1e-4));

  Eigen::MatrixXd mat = 20 * (Eigen::MatrixXd::Random(2, 100).array() + 2);
  Eigen::MatrixXd mat_db;
  Eigen::MatrixXd mat_result;
  PowerRatioToDecibels(mat, &mat_db);
  DecibelsToPowerRatio(mat_db, &mat_result);
  EXPECT_THAT(mat, EigenArrayNear(mat_result, 1e-12));
}

TEST(DecibelsTest, EigenInPlaceTest) {
  {
    Eigen::MatrixXd mat = 20 * (Eigen::MatrixXd::Random(2, 100).array() + 2);
    Eigen::MatrixXd mat_result;
    AmplitudeRatioToDecibels(mat, &mat_result);
    AmplitudeRatioToDecibels(mat, &mat);
    EXPECT_THAT(mat, EigenArrayNear(mat_result, 1e-12));
  }
  {
    Eigen::MatrixXd mat = 20 * (Eigen::MatrixXd::Random(2, 100).array() + 2);
    Eigen::MatrixXd mat_result;
    DecibelsToAmplitudeRatio(mat, &mat_result);
    DecibelsToAmplitudeRatio(mat, &mat);
    EXPECT_THAT(mat, EigenArrayNear(mat_result, 1e-12));
  }
  {
    Eigen::MatrixXd mat = 20 * (Eigen::MatrixXd::Random(2, 100).array() + 2);
    Eigen::MatrixXd mat_result;
    PowerRatioToDecibels(mat, &mat_result);
    PowerRatioToDecibels(mat, &mat);
    EXPECT_THAT(mat, EigenArrayNear(mat_result, 1e-12));
  }
  {
    Eigen::MatrixXd mat = 20 * (Eigen::MatrixXd::Random(2, 100).array() + 2);
    Eigen::MatrixXd mat_result;
    DecibelsToPowerRatio(mat, &mat_result);
    DecibelsToPowerRatio(mat, &mat);
    EXPECT_THAT(mat, EigenArrayNear(mat_result, 1e-12));
  }
}

}  // namespace
}  // namespace audio_dsp

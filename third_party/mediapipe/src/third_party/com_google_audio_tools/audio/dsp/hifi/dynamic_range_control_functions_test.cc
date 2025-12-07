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

#include "audio/dsp/hifi/dynamic_range_control_functions.h"
#include "audio/dsp/testing_util.h"
#include "gtest/gtest.h"
#include "third_party/eigen3/Eigen/Core"

#include "audio/dsp/porting.h"  // auto-added.


namespace audio_dsp {
namespace {

float CheckReferenceCompressor(float input_level_db, float threshold_db,
                               float ratio, float knee_width_db) {
  const float half_knee = knee_width_db / 2;
  if (input_level_db - threshold_db <= -half_knee) {
    return input_level_db;
  } else if (input_level_db - threshold_db >= half_knee) {
    return threshold_db + (input_level_db - threshold_db) / ratio;
  } else {
    const float knee_end = input_level_db - threshold_db + half_knee;
    return input_level_db +
           ((1 / ratio) - 1) * knee_end * knee_end / (knee_width_db * 2);
  }
}

float CheckReferenceExpander(float input_level_db, float threshold_db,
                             float ratio, float knee_width_db) {
  const float half_knee = knee_width_db / 2;
  if (input_level_db - threshold_db >= half_knee) {
    return input_level_db;
  } else if (input_level_db - threshold_db <= -half_knee) {
    return threshold_db + (input_level_db - threshold_db) * ratio;
  } else {
    const float knee_end = input_level_db - threshold_db - half_knee;
    return input_level_db +
           (1 - ratio) * knee_end * knee_end / (knee_width_db * 2);
  }
}

// Helper functions for testing a single scalar at a time. Verifies that
// a reference implementation is matched.
float OutputLevelCompressor(float input, float threshold, float ratio,
                            float knee) {
  Eigen::ArrayXf input_arr = Eigen::ArrayXf::Constant(1, input);
  Eigen::ArrayXf output_arr = Eigen::ArrayXf::Constant(1, 0);
  ::audio_dsp::OutputLevelCompressor(input_arr, threshold, ratio, knee,
                                     &output_arr);
  EXPECT_NEAR(CheckReferenceCompressor(input, threshold, ratio, knee),
              output_arr.value(), 1e-4f);
  return output_arr.value();
}

float OutputLevelLimiter(float input, float threshold, float knee) {
  Eigen::ArrayXf input_arr = Eigen::ArrayXf::Constant(1, input);
  Eigen::ArrayXf output_arr = Eigen::ArrayXf::Constant(1, 0);
  ::audio_dsp::OutputLevelLimiter(input_arr, threshold, knee, &output_arr);
  // A limiter is a compressor with a ratio of infinity.
  EXPECT_NEAR(
      CheckReferenceCompressor(input, threshold,
                               std::numeric_limits<float>::infinity(), knee),
      output_arr.value(), 1e-4f);
  return output_arr.value();
}

float OutputLevelExpander(float input, float threshold, float ratio,
                          float knee) {
  Eigen::ArrayXf input_arr = Eigen::ArrayXf::Constant(1, input);
  Eigen::ArrayXf output_arr = Eigen::ArrayXf::Constant(1, 0);
  ::audio_dsp::OutputLevelExpander(input_arr, threshold, ratio, knee,
                                   &output_arr);
  EXPECT_NEAR(CheckReferenceExpander(input, threshold, ratio, knee),
              output_arr.value(), 1e-4f);
  return output_arr.value();
}

float OutputLevelNoiseGate(float input, float threshold, float knee) {
  Eigen::ArrayXf input_arr = Eigen::ArrayXf::Constant(1, input);
  Eigen::ArrayXf output_arr = Eigen::ArrayXf::Constant(1, 0);
  input_arr[0] = input;
  constexpr float kRatio = 1000.0f;
  ::audio_dsp::OutputLevelExpander(input_arr, threshold, kRatio, knee,
                                   &output_arr);
  return output_arr.value();
}

float OutputLevelTwoWayCompressor(
    float input, const ::audio_dsp::TwoWayCompressionParams& params) {
  Eigen::ArrayXf input_arr = Eigen::ArrayXf::Constant(1, input);
  Eigen::ArrayXf output_arr = Eigen::ArrayXf::Constant(1, 0);
  ::audio_dsp::OutputLevelTwoWayCompressor(input_arr, params, &output_arr);
  return output_arr.value();
}

TEST(ComputeGainTest, CompressorTest) {
  // No knee, above the threshold.
  EXPECT_FLOAT_EQ(OutputLevelCompressor(5.0f, 0.0f, 3.0f, 0.0f), 5.0f / 3.0f);
  EXPECT_FLOAT_EQ(OutputLevelCompressor(5.0f, 0.0f, 6.0f, 0.0f), 5.0f / 6.0f);
  EXPECT_FLOAT_EQ(OutputLevelCompressor(8.0f, 0.0f, 3.0f, 0.0f), 8.0f / 3.0f);
  EXPECT_FLOAT_EQ(OutputLevelCompressor(8.0f, 0.0f, 6.0f, 0.0f), 8.0f / 6.0f);
  EXPECT_FLOAT_EQ(OutputLevelCompressor(5.0f, -1.0f, 3.0f, 0.0f), 1.0f);
  EXPECT_FLOAT_EQ(OutputLevelCompressor(8.0f, -1.0f, 3.0f, 0.0f),
                  -1.0f + 9.0f / 3.0f);
  // No knee, below the threshold, input = output.
  for (float input = -40.0f; input < 20.0; input += 2.0) {
    ASSERT_FLOAT_EQ(OutputLevelCompressor(input, input + 0.1, 3.0f, 0.0f),
                    input);
  }
  // Add a knee and check for...
  for (float input = -50.0f; input < 50.0f; input += 0.1) {
    // Continuity.
    ASSERT_NEAR(OutputLevelCompressor(input, -10.0f, 3.0f, 10.0f),
                OutputLevelCompressor(input + 0.1, -10.0f, 3.0f, 10.0f),
                0.1 + 1e-5);
    // Monotonic decrease as knee increases.
    for (float knee_db = 0.5f; knee_db < 30.0f; knee_db += 1.5) {
      float new_knee = 10.0 + knee_db;
      ASSERT_GE(OutputLevelCompressor(input, -10.0f, 3.0f, 10.0f),
                OutputLevelCompressor(input, -10.0f, 3.0f, new_knee) - 1e-5);
    }
  }

  // Check that knee kicks in at the right place.
  for (float knee : {4.0, 8.0, 12.0}) {
    float half_knee = knee / 2;
    EXPECT_FLOAT_EQ(OutputLevelCompressor(-half_knee, 0.0f, 3.0f, 0.0f),
                    OutputLevelCompressor(-half_knee, 0.0f, 3.0f, knee));
    EXPECT_GT(
        std::abs(OutputLevelCompressor(-(half_knee - 0.2f), 0.0f, 3.0f, 0.0f) -
                 OutputLevelCompressor(-(half_knee - 0.2f), 0.0f, 3.0f, knee)),
        1e-3);
    EXPECT_GT(std::abs(OutputLevelCompressor(0.0f, 0.0f, 3.0f, 0.0f) -
                       OutputLevelCompressor(0.0f, 0.0f, 3.0f, knee)),
              3e-1);
    EXPECT_GT(
        std::abs(OutputLevelCompressor(half_knee - 0.2f, 0.0f, 3.0f, 0.0f) -
                 OutputLevelCompressor(half_knee - 0.2f, 0.0f, 3.0f, knee)),
        1e-3);
    EXPECT_FLOAT_EQ(OutputLevelCompressor(half_knee, 0.0f, 3.0f, 0.0f),
                    OutputLevelCompressor(half_knee, 0.0f, 3.0f, knee));
  }
}

TEST(ComputeGainTest, LimiterTest) {
  // No knee, above the threshold.
  EXPECT_FLOAT_EQ(OutputLevelLimiter(5.0f, 0.0f, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(OutputLevelLimiter(5.0f, 0.0f, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(OutputLevelLimiter(8.0f, 0.0f, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(OutputLevelLimiter(8.0f, 0.0f, 0.0f), 0.0f);
  EXPECT_FLOAT_EQ(OutputLevelLimiter(5.0f, -1.0f, 0.0f), -1.0f);
  EXPECT_FLOAT_EQ(OutputLevelLimiter(8.0f, -1.0f, 0.0f), -1.0f);
  // No knee, below the threshold, input = output.
  for (float input = -40.0f; input < 20.0; input += 2.0) {
    ASSERT_FLOAT_EQ(OutputLevelLimiter(input, input + 0.1, 0.0f), input);
  }
  // No knee, above the threshold, input = threshold.
  for (float input = -40.0f; input < 20.0; input += 2.0) {
    float threshold_db = input - 0.1;
    ASSERT_FLOAT_EQ(OutputLevelLimiter(input, threshold_db, 0.0f),
                    threshold_db);
  }
  // Add a knee and check for...
  for (float input = -50.0f; input < 50.0f; input += 0.1) {
    // Continuity.
    ASSERT_NEAR(OutputLevelLimiter(input, -10.0f, 10.0f),
                OutputLevelLimiter(input + 0.1, -10.0f, 10.0f), 0.1 + 1e-5);
    // Monotonic decrease as knee increases.
    for (float knee_db = 0.5f; knee_db < 30.0f; knee_db += 1.5) {
      float new_knee = 10.0 + knee_db;
      ASSERT_GE(OutputLevelLimiter(input, -10.0f, 10.0f),
                OutputLevelLimiter(input, -10.0f, new_knee) - 1e-5);
    }
  }
  // Check that knee kicks in at the right place.
  for (float knee : {4.0, 8.0, 12.0}) {
    float half_knee = knee / 2;
    EXPECT_FLOAT_EQ(OutputLevelLimiter(-half_knee, 0.0f, 0.0f),
                    OutputLevelLimiter(-half_knee, 0.0f, knee));
    EXPECT_GT(std::abs(OutputLevelLimiter(-(half_knee - 0.2f), 0.0f, 0.0f) -
                       OutputLevelLimiter(-(half_knee - 0.2f), 0.0f, knee)),
              1e-3);
    EXPECT_GT(std::abs(OutputLevelLimiter(0.0f, 0.0f, 0.0f) -
                       OutputLevelLimiter(0.0f, 0.0f, knee)),
              3e-1);
    EXPECT_GT(std::abs(OutputLevelLimiter(half_knee - 0.2f, 0.0f, 0.0f) -
                       OutputLevelLimiter(half_knee - 0.2f, 0.0f, knee)),
              1e-3);
    EXPECT_FLOAT_EQ(OutputLevelLimiter(half_knee, 0.0f, 0.0f),
                    OutputLevelLimiter(half_knee, 0.0f, knee));
  }
}

TEST(ComputeGainTest, ExpanderTest) {
  // No knee, below the threshold.
  EXPECT_FLOAT_EQ(OutputLevelExpander(-5.0f, 0.0f, 3.0f, 0.0f), -15.0f);
  EXPECT_FLOAT_EQ(OutputLevelExpander(-5.0f, 0.0f, 6.0f, 0.0f), -30.0f);
  EXPECT_FLOAT_EQ(OutputLevelExpander(-8.0f, 0.0f, 3.0f, 0.0f), -24.0f);
  EXPECT_FLOAT_EQ(OutputLevelExpander(-8.0f, 0.0f, 6.0f, 0.0f), -48.0f);
  EXPECT_FLOAT_EQ(OutputLevelExpander(-5.0f, -1.0f, 3.0f, 0.0f), -13.0f);
  EXPECT_FLOAT_EQ(OutputLevelExpander(-8.0f, -1.0f, 3.0f, 0.0f), -22.0f);
  // No knee, below the threshold, input = output.
  for (float input = -40.0f; input < 20.0; input += 2.0) {
    ASSERT_FLOAT_EQ(OutputLevelExpander(input, input - 0.1, 3.0f, 0.0f), input);
  }
  // Add a knee and check for...
  for (float input = -50.0f; input < 50.0f; input += 0.1) {
    // Continuity.
    ASSERT_NEAR(OutputLevelExpander(input, -10.0f, 3.0f, 10.0f),
                OutputLevelExpander(input + 0.1, -10.0f, 3.0f, 10.0f),
                0.3 /* ratio * 0.1 */ + 1e-5);
    // Monotonic decrease as knee increases.
    for (float knee_db = 0.5f; knee_db < 30.0f; knee_db += 1.5) {
      float new_knee = 10.0 + knee_db;
      ASSERT_GE(OutputLevelExpander(input, -10.0f, 3.0f, 10.0f),
                OutputLevelExpander(input, -10.0f, 3.0f, new_knee) - 2e-5);
    }
  }

  // Check that knee kicks in at the right place.
  for (float knee : {4.0, 8.0, 12.0}) {
    float half_knee = knee / 2;
    EXPECT_FLOAT_EQ(OutputLevelExpander(-half_knee, 0.0f, 3.0f, 0.0f),
                    OutputLevelExpander(-half_knee, 0.0f, 3.0f, knee));
    EXPECT_GT(
        std::abs(OutputLevelExpander(-(half_knee - 0.2f), 0.0f, 3.0f, 0.0f) -
                 OutputLevelExpander(-(half_knee - 0.2f), 0.0f, 3.0f, knee)),
        1e-3);
    EXPECT_GT(std::abs(OutputLevelExpander(0.0f, 0.0f, 3.0f, 0.0f) -
                       OutputLevelExpander(0.0f, 0.0f, 3.0f, knee)),
              3e-1);
    EXPECT_GT(std::abs(OutputLevelExpander(half_knee - 0.2f, 0.0f, 3.0f, 0.0f) -
                       OutputLevelExpander(half_knee - 0.2f, 0.0f, 3.0f, knee)),
              1e-3);
    EXPECT_FLOAT_EQ(OutputLevelExpander(half_knee, 0.0f, 3.0f, 0.0f),
                    OutputLevelExpander(half_knee, 0.0f, 3.0f, knee));
  }
}

TEST(ComputeGainTest, NoiseGateTest) {
  // No knee, below the threshold.
  EXPECT_LT(OutputLevelNoiseGate(-5.0f, 0.0f, 0.0f), -140.0f);
  EXPECT_LT(OutputLevelNoiseGate(-5.0f, 0.0f, 0.0f), -140.0f);
  EXPECT_LT(OutputLevelNoiseGate(-8.0f, 0.0f, 0.0f), -140.0f);
  EXPECT_LT(OutputLevelNoiseGate(-8.0f, 0.0f, 0.0f), -140.0f);
  EXPECT_LT(OutputLevelNoiseGate(-5.0f, -1.0f, 0.0f), -140.0f);
  EXPECT_LT(OutputLevelNoiseGate(-8.0f, -1.0f, 0.0f), -140.0f);
  // // No knee, above the threshold, input = output.
  for (float input = -40.0f; input < 20.0; input += 2.0) {
    EXPECT_FLOAT_EQ(OutputLevelNoiseGate(input, input - 0.1, 0.0f), input);
    EXPECT_LT(OutputLevelNoiseGate(input, input + 0.4, 0.0f), -140.0f);
  }
}

TEST(ComputeGainTest, TwoWayCompressorTestUnityRatioRegion) {
  // Configure the nonlinearity as a unity gain curve. All thresholds set to the
  // same point, all ratios set to 1.0.
  ::audio_dsp::TwoWayCompressionParams params;

  params.soft_compressor_region.threshold_db = -100.0;
  params.soft_compressor_region.ratio = 1.0;
  params.hard_compressor_region.threshold_db = -100.0;
  params.hard_compressor_region.ratio = 1.0;
  params.upwards_compressor_region.threshold_db = -100.0;
  params.upwards_compressor_region.ratio = 1.0;
  params.expander_region.threshold_db = -100.0;
  params.expander_region.ratio = 1.0;

  // At, above, and below threshold, input = output.
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-100.0, params), -100.0);
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(0.0, params), 0.0);
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-200.0, params), -200.0);
}

TEST(ComputeGainTest, TwoWayCompressorTestSoftCompressorRegion) {
  // Configure the nonlinearity as a single downwards compressor using the soft
  // compressor. Ratios for expander and upwards compressor set to 1.0. Soft and
  // hard compressor have the same ratios.
  ::audio_dsp::TwoWayCompressionParams params;

  params.soft_compressor_region.threshold_db = -10.0;
  params.soft_compressor_region.ratio = 2.0;
  params.hard_compressor_region.threshold_db = 0.0;
  params.hard_compressor_region.ratio = params.soft_compressor_region.ratio;
  params.upwards_compressor_region.threshold_db = -100.0;
  params.upwards_compressor_region.ratio = 1.0;
  params.expander_region.threshold_db = -100.0;
  params.expander_region.ratio = 1.0;

  // Equivalence to a single downwards compressor.
  for (float knee : {0.0, 4.0, 8.0}) {
    params.soft_compressor_region.knee_width_db = knee;
    for (float input = -50.0; input < 0.0; input += 0.5) {
      EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-5.0, params),
                      OutputLevelCompressor(
                          -5.0f, params.soft_compressor_region.threshold_db,
                          params.soft_compressor_region.ratio,
                          params.soft_compressor_region.knee_width_db));
    }
  }
}

TEST(ComputeGainTest, TwoWayCompressorTestUpwardsCompressorRegion) {
  // Configure the nonlinearity as a single upwards compressor. Ratios for
  // downwards compressors and expander set to 1.0. The expander must have a
  // ratio of at least 1.0, so its threshold is set far below the threshold of
  // the upwards compressor.
  ::audio_dsp::TwoWayCompressionParams params;

  params.soft_compressor_region.threshold_db = 0.0;
  params.soft_compressor_region.ratio = 1.0;
  params.hard_compressor_region.threshold_db = 0.0;
  params.hard_compressor_region.ratio = 1.0;
  params.upwards_compressor_region.threshold_db = -20.0;
  params.upwards_compressor_region.ratio = 2.0;
  params.expander_region.threshold_db = -1000.0;
  params.expander_region.ratio = 1.0;

  // Equivalence to a single expander configured as an upwards compressor.
  for (float knee : {0.0, 4.0, 8.0}) {
    params.upwards_compressor_region.knee_width_db = knee;
    for (float input = 0.0; input > -50.0; input -= 0.5) {
      EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(input, params),
                      OutputLevelExpander(
                          input, params.upwards_compressor_region.threshold_db,
                          1.0 / params.upwards_compressor_region.ratio,
                          params.upwards_compressor_region.knee_width_db));
    }
  }
}

TEST(ComputeGainTest, TwoWayCompressorTestHardCompressorRegion) {
  // Configure the nonlinearity as a single downwards compressor with a hard
  // compressor. The ratios for the soft compressor, upwards compressor, and
  // expander are set to 1.0, and all of their thresholds are equal.
  ::audio_dsp::TwoWayCompressionParams params;

  params.soft_compressor_region.threshold_db = -100.0;
  params.soft_compressor_region.ratio = 1.0;
  params.hard_compressor_region.threshold_db = -5.0;
  params.hard_compressor_region.ratio = 10.0;
  params.upwards_compressor_region.threshold_db = -100.0;
  params.upwards_compressor_region.ratio = 1.0;
  params.expander_region.threshold_db = -100.0;
  params.expander_region.ratio = 1.0;

  // Equivalence to a single downwards compressor.
  for (float knee : {0.0, 4.0, 8.0}) {
    params.hard_compressor_region.knee_width_db = knee;
    for (float input = -50.0; input < 0.0; input += 0.5) {
      EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(input, params),
                      OutputLevelCompressor(
                          input, params.hard_compressor_region.threshold_db,
                          params.hard_compressor_region.ratio,
                          params.hard_compressor_region.knee_width_db));
    }
  }
}

TEST(ComputeGainTest, TwoWayCompressorTestExpanderRegion) {
  // Configure the nonlinearity as a single expander. The ratios for the upwards
  // compressor, soft compressor, and hard compressor are set to 1.0, and they
  // all have the same threshold.
  ::audio_dsp::TwoWayCompressionParams params;

  params.soft_compressor_region.threshold_db = 0.0;
  params.soft_compressor_region.ratio = 1.0;
  params.hard_compressor_region.threshold_db = 0.0;
  params.hard_compressor_region.ratio = 1.0;
  params.upwards_compressor_region.threshold_db = 0.0;
  params.upwards_compressor_region.ratio = 1.0;
  params.expander_region.threshold_db = -25.0;
  params.expander_region.ratio = 2.0;

  // Equivalence to a single expander.
  for (float knee : {0.0, 4.0, 8.0}) {
    params.expander_region.knee_width_db = knee;
    for (float input = 0.0; input > -50.0; input -= 0.5) {
      EXPECT_FLOAT_EQ(
          OutputLevelTwoWayCompressor(input, params),
          OutputLevelExpander(input, params.expander_region.threshold_db,
                              params.expander_region.ratio,
                              params.expander_region.knee_width_db));
    }
  }
}

TEST(ComputeGainTest, TwoWayCompressorTestFullProfile) {
  // Configure the nonlinearity to have an expander, a upwards compressor, a
  // soft compressor, and a hard compressor. There are hard knees at every
  // transition.
  ::audio_dsp::TwoWayCompressionParams params_hard_knee;

  params_hard_knee.soft_compressor_region.threshold_db = -10.0;
  params_hard_knee.soft_compressor_region.ratio = 2.0;
  params_hard_knee.hard_compressor_region.threshold_db = -5.0;
  params_hard_knee.hard_compressor_region.ratio = 10.0;
  params_hard_knee.upwards_compressor_region.threshold_db = -20.0;
  params_hard_knee.upwards_compressor_region.ratio = 2.0;
  params_hard_knee.expander_region.threshold_db = -25.0;
  params_hard_knee.expander_region.ratio = 2.0;

  ::audio_dsp::TwoWayCompressionParams params_soft_knee = params_hard_knee;

  // The unity gain region of the nonlinearity. Input = output.
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-15.0, params_hard_knee), -15.0);

  // The soft compressor region of the nonlinearity.
  // At threshold, input = output.
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-10.0, params_hard_knee), -10);
  // Above threshold.
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-8.0, params_hard_knee), -9.0);

  // The hard compressor region of the nonlinearity.
  // At threshold.
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-5.0, params_hard_knee), -7.5);
  // Above threshold.
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-2.0, params_hard_knee), -7.2);

  // The upwards compressor region of the nonlinearity.
  // At threshold, input = output.
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-20.0, params_hard_knee), -20.0);
  // Below threshold.
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-22.0, params_hard_knee), -21.0);

  // The expander region of the nonlinearity.
  // At threshold.
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-25.0, params_hard_knee), -22.5);
  // Below threshold.
  EXPECT_FLOAT_EQ(OutputLevelTwoWayCompressor(-27.0, params_hard_knee), -26.5);

  // Test that the knees move the curve at the right transition point
  // for the soft compressor region.
  for (float knee : {4.0, 8.0, 12.0}) {
    float knee_start =
        params_soft_knee.soft_compressor_region.threshold_db - (knee / 2.0f);
    float knee_end = knee_start + knee;
    EXPECT_FLOAT_EQ(
        OutputLevelTwoWayCompressor(knee_start - 0.1, params_hard_knee),
        OutputLevelTwoWayCompressor(knee_start - 0.1, params_soft_knee));
    EXPECT_FLOAT_EQ(
        OutputLevelTwoWayCompressor(knee_end + 0.1, params_hard_knee),
        OutputLevelTwoWayCompressor(knee_end + 0.1, params_soft_knee));
    for (float input = knee_start; input <= knee_end; input += 0.1) {
      EXPECT_LE(OutputLevelTwoWayCompressor(input, params_soft_knee),
                OutputLevelTwoWayCompressor(input, params_hard_knee));
    }
  }

  // Test that the knees move the curve at the right transition point
  // for the hard compressor region.
  for (float knee : {4.0, 8.0, 12.0}) {
    float knee_start =
        params_soft_knee.hard_compressor_region.threshold_db - (knee / 2.0f);
    float knee_end = knee_start + knee;
    EXPECT_FLOAT_EQ(
        OutputLevelTwoWayCompressor(knee_start - 0.1, params_hard_knee),
        OutputLevelTwoWayCompressor(knee_start - 0.1, params_soft_knee));
    EXPECT_FLOAT_EQ(
        OutputLevelTwoWayCompressor(knee_end + 0.1, params_hard_knee),
        OutputLevelTwoWayCompressor(knee_end + 0.1, params_soft_knee));
    for (float input = knee_start; input <= knee_end; input += 0.1) {
      EXPECT_LE(OutputLevelTwoWayCompressor(input, params_soft_knee),
                OutputLevelTwoWayCompressor(input, params_hard_knee));
    }
  }

  // Test that the knees move the curve at the right transition point
  // for the upwards compressor region.
  for (float knee : {4.0, 8.0, 12.0}) {
    float knee_start =
        params_soft_knee.upwards_compressor_region.threshold_db - (knee / 2.0f);
    float knee_end = knee_start + knee;
    EXPECT_FLOAT_EQ(
        OutputLevelTwoWayCompressor(knee_start - 0.1, params_hard_knee),
        OutputLevelTwoWayCompressor(knee_start - 0.1, params_soft_knee));
    EXPECT_FLOAT_EQ(
        OutputLevelTwoWayCompressor(knee_end + 0.1, params_hard_knee),
        OutputLevelTwoWayCompressor(knee_end + 0.1, params_soft_knee));
    for (float input = knee_start; input <= knee_end; input += 0.1) {
      EXPECT_GE(OutputLevelTwoWayCompressor(input, params_soft_knee),
                OutputLevelTwoWayCompressor(input, params_hard_knee));
    }
  }

  // Test that the knees move the curve at the right transition point
  // for the expander region.
  for (float knee : {4.0, 8.0, 12.0}) {
    float knee_start =
        params_soft_knee.expander_region.threshold_db - (knee / 2.0f);
    float knee_end = knee_start + knee;
    EXPECT_FLOAT_EQ(
        OutputLevelTwoWayCompressor(knee_start - 0.1, params_hard_knee),
        OutputLevelTwoWayCompressor(knee_start - 0.1, params_soft_knee));
    EXPECT_FLOAT_EQ(
        OutputLevelTwoWayCompressor(knee_end + 0.1, params_hard_knee),
        OutputLevelTwoWayCompressor(knee_end + 0.1, params_soft_knee));
    for (float input = knee_start; input <= knee_end; input += 0.1) {
      EXPECT_LE(OutputLevelTwoWayCompressor(input, params_soft_knee),
                OutputLevelTwoWayCompressor(input, params_hard_knee));
    }
  }
}

}  // namespace
}  // namespace audio_dsp

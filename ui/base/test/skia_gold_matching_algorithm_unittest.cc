// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_matching_algorithm.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace test {
class SkiaGoldMatchingAlgorithmTest : public ::testing::Test {};

TEST_F(SkiaGoldMatchingAlgorithmTest, ExactMatching) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  ExactSkiaGoldMatchingAlgorithm algorithm;
  algorithm.AppendAlgorithmToCmdline(cmd);
  EXPECT_EQ(
      cmd.GetArgumentsString().find(FILE_PATH_LITERAL("add-test-optional-key")),
      base::CommandLine::StringType::npos);
}

TEST_F(SkiaGoldMatchingAlgorithmTest, FuzzyMatching) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  FuzzySkiaGoldMatchingAlgorithm algorithm(1, 2, 3);
  algorithm.AppendAlgorithmToCmdline(cmd);
  EXPECT_NE(cmd.GetArgumentsString().find(FILE_PATH_LITERAL(
                "--add-test-optional-key=image_matching_algorithm:fuzzy")),
            base::CommandLine::StringType::npos);
  EXPECT_NE(cmd.GetArgumentsString().find(FILE_PATH_LITERAL(
                "--add-test-optional-key=fuzzy_max_different_pixels:1")),
            base::CommandLine::StringType::npos);
  EXPECT_NE(cmd.GetArgumentsString().find(FILE_PATH_LITERAL(
                "--add-test-optional-key=fuzzy_pixel_delta_threshold:2")),
            base::CommandLine::StringType::npos);
  EXPECT_NE(cmd.GetArgumentsString().find(FILE_PATH_LITERAL(
                "--add-test-optional-key=fuzzy_ignored_border_thickness:3")),
            base::CommandLine::StringType::npos);
}

TEST_F(SkiaGoldMatchingAlgorithmTest, SobelMatching) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  SobelSkiaGoldMatchingAlgorithm algorithm(1, 2, 3, 4);
  algorithm.AppendAlgorithmToCmdline(cmd);
  EXPECT_NE(cmd.GetArgumentsString().find(FILE_PATH_LITERAL(
                "--add-test-optional-key=image_matching_algorithm:sobel")),
            base::CommandLine::StringType::npos);
  EXPECT_NE(cmd.GetArgumentsString().find(FILE_PATH_LITERAL(
                "--add-test-optional-key=fuzzy_max_different_pixels:1")),
            base::CommandLine::StringType::npos);
  EXPECT_NE(cmd.GetArgumentsString().find(FILE_PATH_LITERAL(
                "--add-test-optional-key=fuzzy_pixel_delta_threshold:2")),
            base::CommandLine::StringType::npos);
  EXPECT_NE(cmd.GetArgumentsString().find(FILE_PATH_LITERAL(
                "--add-test-optional-key=sobel_edge_threshold:3")),
            base::CommandLine::StringType::npos);
  EXPECT_NE(cmd.GetArgumentsString().find(FILE_PATH_LITERAL(
                "--add-test-optional-key=fuzzy_ignored_border_thickness:4")),
            base::CommandLine::StringType::npos);
}

TEST_F(SkiaGoldMatchingAlgorithmTest, InvalidInput) {
  EXPECT_DCHECK_DEATH(SobelSkiaGoldMatchingAlgorithm(-1, 2, 3, 4));
  EXPECT_DCHECK_DEATH(SobelSkiaGoldMatchingAlgorithm(1, -1, 3, 4));
  EXPECT_DCHECK_DEATH(SobelSkiaGoldMatchingAlgorithm(1, 2, -1, 4));
  EXPECT_DCHECK_DEATH(SobelSkiaGoldMatchingAlgorithm(1, 2, 3, -1));
  EXPECT_DCHECK_DEATH(SobelSkiaGoldMatchingAlgorithm(1, 2, 256, 4));
}

}  // namespace test
}  // namespace ui

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_pixel_diff.h"

#include "base/command_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::HasSubstr;
using ::testing::Property;

namespace ui {
namespace test {

class MockSkiaGoldPixelDiff : public SkiaGoldPixelDiff {
 public:
  MockSkiaGoldPixelDiff() = default;
  MOCK_CONST_METHOD1(LaunchProcess, int(const base::CommandLine&));
};

class SkiaGoldPixelDiffTest : public ::testing::Test {
 public:
  SkiaGoldPixelDiffTest() {
    auto* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII("git-revision", "test");
  }

  ~SkiaGoldPixelDiffTest() override {}

 protected:
  DISALLOW_COPY_AND_ASSIGN(SkiaGoldPixelDiffTest);
};

TEST_F(SkiaGoldPixelDiffTest, CompareScreenshotBySkBitmap) {
  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(3);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", bitmap);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, BypassSkiaGoldFunctionality) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "bypass-skia-gold-functionality");

  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(0);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", bitmap);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, FuzzyMatching) {
  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_pixel,
      LaunchProcess(AllOf(
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("image_matching_algorithm:fuzzy"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("fuzzy_max_different_pixels:1"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("fuzzy_pixel_delta_threshold:2"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  FuzzySkiaGoldMatchingAlgorithm algorithm(1, 2);
  bool ret = mock_pixel.CompareScreenshot("test", bitmap, &algorithm);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, FuzzyMatchingWithIgnoredBorder) {
  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_pixel,
      LaunchProcess(AllOf(
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("image_matching_algorithm:fuzzy"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("fuzzy_max_different_pixels:1"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("fuzzy_pixel_delta_threshold:2"))),
          Property(&base::CommandLine::GetCommandLineString,
                   HasSubstr(FILE_PATH_LITERAL(
                       "fuzzy_ignored_border_thickness:3"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  FuzzySkiaGoldMatchingAlgorithm algorithm(1, 2, 3);
  bool ret = mock_pixel.CompareScreenshot("test", bitmap, &algorithm);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, SobelMatching) {
  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_pixel,
      LaunchProcess(AllOf(
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("image_matching_algorithm:sobel"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("fuzzy_max_different_pixels:1"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("fuzzy_pixel_delta_threshold:2"))),
          Property(&base::CommandLine::GetCommandLineString,
                   HasSubstr(FILE_PATH_LITERAL("sobel_edge_threshold:3"))),
          Property(&base::CommandLine::GetCommandLineString,
                   HasSubstr(FILE_PATH_LITERAL(
                       "fuzzy_ignored_border_thickness:4"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  SobelSkiaGoldMatchingAlgorithm algorithm(1, 2, 3, 4);
  bool ret = mock_pixel.CompareScreenshot("test", bitmap, &algorithm);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, DefaultCorpus) {
  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_pixel,
      LaunchProcess(AllOf(Property(
          &base::CommandLine::GetCommandLineString,
          HasSubstr(FILE_PATH_LITERAL("gtest-pixeltests"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", bitmap);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, ExplicitCorpus) {
  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(mock_pixel,
              LaunchProcess(AllOf(Property(
                  &base::CommandLine::GetCommandLineString,
                  HasSubstr(FILE_PATH_LITERAL("corpus"))))))
      .Times(1);
  mock_pixel.Init("Prefix", "corpus");
  bool ret = mock_pixel.CompareScreenshot("test", bitmap);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, DefaultCodeReviewSystem) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitchASCII("gerrit-issue", "1");
  cmd_line->AppendSwitchASCII("gerrit-patchset", "2");
  cmd_line->AppendSwitchASCII("buildbucket-id", "3");

  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_pixel,
      LaunchProcess(AllOf(Property(&base::CommandLine::GetCommandLineString,
                                   HasSubstr(FILE_PATH_LITERAL("gerrit"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", bitmap);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, ExplicitCodeReviewSystem) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitchASCII("gerrit-issue", "1");
  cmd_line->AppendSwitchASCII("gerrit-patchset", "2");
  cmd_line->AppendSwitchASCII("buildbucket-id", "3");
  cmd_line->AppendSwitchASCII("code-review-system", "new-crs");

  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(mock_pixel,
              LaunchProcess(
                  AllOf(Property(&base::CommandLine::GetCommandLineString,
                                 HasSubstr(FILE_PATH_LITERAL("new-crs"))),
                        Property(&base::CommandLine::GetCommandLineString,
                                 Not(HasSubstr(FILE_PATH_LITERAL("gerrit")))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", bitmap);
  EXPECT_TRUE(ret);
}

}  // namespace test
}  // namespace ui

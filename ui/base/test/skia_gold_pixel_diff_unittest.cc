// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_pixel_diff.h"

#include "base/command_line.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/test_switches.h"
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
    CreateTestBitmap();
  }

  SkiaGoldPixelDiffTest(const SkiaGoldPixelDiffTest&) = delete;
  SkiaGoldPixelDiffTest& operator=(const SkiaGoldPixelDiffTest&) = delete;

  ~SkiaGoldPixelDiffTest() override {}

  SkBitmap GetTestBitmap() { return test_bitmap_; }
  void CreateTestBitmap() {
    SkImageInfo info =
        SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                          SkAlphaType::kPremul_SkAlphaType);
    test_bitmap_.allocPixels(info, 10 * 4);
  }

 protected:

 private:
  SkBitmap test_bitmap_;
};

TEST_F(SkiaGoldPixelDiffTest, CompareScreenshotBySkBitmap) {
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(3);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, BypassSkiaGoldFunctionality) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "bypass-skia-gold-functionality");
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(0);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, LuciAuthSwitch) {
  MockSkiaGoldPixelDiff mock_pixel;
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch(switches::kTestLauncherBotMode);

  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_pixel,
      LaunchProcess(AllOf(Property(&base::CommandLine::GetCommandLineString,
                                   HasSubstr(FILE_PATH_LITERAL("--luci"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, NoLuciAuthSwitch) {
  MockSkiaGoldPixelDiff mock_pixel;
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch("no-luci-auth");

  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(mock_pixel, LaunchProcess(AllOf(Property(
                              &base::CommandLine::GetCommandLineString,
                              Not(HasSubstr(FILE_PATH_LITERAL("--luci")))))))
      .Times(3);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, LocalNoLuciAuth) {
  MockSkiaGoldPixelDiff mock_pixel;
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->RemoveSwitch(switches::kTestLauncherBotMode);
  base::ScopedEnvironmentVariableOverride env_override(
      "CHROMIUM_TEST_LAUNCHER_BOT_MODE");

  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(mock_pixel, LaunchProcess(AllOf(Property(
                              &base::CommandLine::GetCommandLineString,
                              Not(HasSubstr(FILE_PATH_LITERAL("--luci")))))))
      .Times(3);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, FuzzyMatching) {
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_pixel,
      LaunchProcess(AllOf(
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL(
                  "--add-test-optional-key=image_matching_algorithm:fuzzy"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL(
                  "--add-test-optional-key=fuzzy_max_different_pixels:1"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL(
                  "--add-test-optional-key=fuzzy_pixel_delta_threshold:2"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  FuzzySkiaGoldMatchingAlgorithm algorithm(1, 2);
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap(), &algorithm);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, FuzzyMatchingWithIgnoredBorder) {
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_pixel,
      LaunchProcess(AllOf(
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL(
                  "--add-test-optional-key=image_matching_algorithm:fuzzy"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL(
                  "--add-test-optional-key=fuzzy_max_different_pixels:1"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL(
                  "--add-test-optional-key=fuzzy_pixel_delta_threshold:2"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("--add-test-optional-key=fuzzy_"
                                          "ignored_border_thickness:3"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  FuzzySkiaGoldMatchingAlgorithm algorithm(1, 2, 3);
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap(), &algorithm);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, SobelMatching) {
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_pixel,
      LaunchProcess(AllOf(
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL(
                  "--add-test-optional-key=image_matching_algorithm:sobel"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL(
                  "--add-test-optional-key=fuzzy_max_different_pixels:1"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL(
                  "--add-test-optional-key=fuzzy_pixel_delta_threshold:2"))),
          Property(&base::CommandLine::GetCommandLineString,
                   HasSubstr(FILE_PATH_LITERAL(
                       "--add-test-optional-key=sobel_edge_threshold:3"))),
          Property(
              &base::CommandLine::GetCommandLineString,
              HasSubstr(FILE_PATH_LITERAL("--add-test-optional-key=fuzzy_"
                                          "ignored_border_thickness:4"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  SobelSkiaGoldMatchingAlgorithm algorithm(1, 2, 3, 4);
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap(), &algorithm);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, DefaultCorpus) {
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(mock_pixel,
              LaunchProcess(AllOf(Property(
                  &base::CommandLine::GetCommandLineString,
                  HasSubstr(FILE_PATH_LITERAL("--corpus=gtest-pixeltests"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, ExplicitCorpus) {
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(mock_pixel,
              LaunchProcess(AllOf(
                  Property(&base::CommandLine::GetCommandLineString,
                           HasSubstr(FILE_PATH_LITERAL("--corpus=corpus"))))))
      .Times(1);
  mock_pixel.Init("Prefix", "corpus");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, DefaultCodeReviewSystem) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitchASCII("gerrit-issue", "1");
  cmd_line->AppendSwitchASCII("gerrit-patchset", "2");
  cmd_line->AppendSwitchASCII("buildbucket-id", "3");

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(mock_pixel, LaunchProcess(AllOf(Property(
                              &base::CommandLine::GetCommandLineString,
                              HasSubstr(FILE_PATH_LITERAL("--crs=gerrit"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, ExplicitCodeReviewSystem) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitchASCII("gerrit-issue", "1");
  cmd_line->AppendSwitchASCII("gerrit-patchset", "2");
  cmd_line->AppendSwitchASCII("buildbucket-id", "3");
  cmd_line->AppendSwitchASCII("code-review-system", "new-crs");

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(mock_pixel,
              LaunchProcess(
                  AllOf(Property(&base::CommandLine::GetCommandLineString,
                                 HasSubstr(FILE_PATH_LITERAL("--crs=new-crs"))),
                        Property(&base::CommandLine::GetCommandLineString,
                                 Not(HasSubstr(FILE_PATH_LITERAL("gerrit")))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, DryRunLocally) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->RemoveSwitch(switches::kTestLauncherBotMode);
  base::ScopedEnvironmentVariableOverride env_override(
      "CHROMIUM_TEST_LAUNCHER_BOT_MODE");

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_pixel,
      LaunchProcess(AllOf(Property(&base::CommandLine::GetCommandLineString,
                                   HasSubstr(FILE_PATH_LITERAL("--dryrun"))))))
      .Times(1);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, NotDryRunOnBots) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch(switches::kTestLauncherBotMode);

  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(AnyNumber());
  EXPECT_CALL(mock_pixel, LaunchProcess(AllOf(Property(
                              &base::CommandLine::GetCommandLineString,
                              Not(HasSubstr(FILE_PATH_LITERAL("--dryrun")))))))
      .Times(3);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

}  // namespace test
}  // namespace ui

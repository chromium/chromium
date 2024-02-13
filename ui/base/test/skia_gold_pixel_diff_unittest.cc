// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_pixel_diff.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/test_switches.h"
#include "base/threading/thread.h"
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

class SkiaGoldPixelDiffTest : public ::testing::Test {
 public:
  using MockLaunchProcess =
      testing::MockFunction<SkiaGoldPixelDiff::LaunchProcessCallback::RunType>;

  SkiaGoldPixelDiffTest() {
    auto* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII("git-revision", "test");
    CreateTestBitmap();
  }

  SkiaGoldPixelDiffTest(const SkiaGoldPixelDiffTest&) = delete;
  SkiaGoldPixelDiffTest& operator=(const SkiaGoldPixelDiffTest&) = delete;

  ~SkiaGoldPixelDiffTest() override = default;

  SkBitmap GetTestBitmap() { return test_bitmap_; }
  void CreateTestBitmap() {
    SkImageInfo info =
        SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                          SkAlphaType::kPremul_SkAlphaType);
    test_bitmap_.allocPixels(info, 10 * 4);
  }

 protected:
  void SetUp() override {
    session_cache_.emplace();
    mock_launch_process_.emplace();
    auto_reset_custom_launch_process_.emplace(
        SkiaGoldPixelDiff::OverrideLaunchProcessForTesting(
            base::BindLambdaForTesting(mock_launch_process_->AsStdFunction())));
  }

  void TearDown() override {
    auto_reset_custom_launch_process_.reset();
    mock_launch_process_.reset();
    session_cache_.reset();
  }

  MockLaunchProcess& mock_launch_process() {
    return mock_launch_process_.value();
  }

 private:
  SkBitmap test_bitmap_;

  std::optional<SkiaGoldPixelDiff::ScopedSessionCacheForTesting> session_cache_;
  std::optional<MockLaunchProcess> mock_launch_process_;
  std::optional<base::AutoReset<SkiaGoldPixelDiff::LaunchProcessCallback>>
      auto_reset_custom_launch_process_;
};

TEST_F(SkiaGoldPixelDiffTest, CompareScreenshotBySkBitmap) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(3);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, BypassSkiaGoldFunctionality) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "bypass-skia-gold-functionality");

  EXPECT_CALL(mock_launch_process(), Call(_)).Times(0);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, LuciAuthSwitch) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch(switches::kTestLauncherBotMode);

  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(mock_launch_process(),
              Call(AllOf(Property(&base::CommandLine::GetCommandLineString,
                                  HasSubstr(FILE_PATH_LITERAL("--luci"))))))
      .Times(1);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, NoLuciAuthSwitch) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch("no-luci-auth");

  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_launch_process(),
      Call(AllOf(Property(&base::CommandLine::GetCommandLineString,
                          Not(HasSubstr(FILE_PATH_LITERAL("--luci")))))))
      .Times(3);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, LocalNoLuciAuth) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->RemoveSwitch(switches::kTestLauncherBotMode);
  base::ScopedEnvironmentVariableOverride env_override(
      "CHROMIUM_TEST_LAUNCHER_BOT_MODE");

  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_launch_process(),
      Call(AllOf(Property(&base::CommandLine::GetCommandLineString,
                          Not(HasSubstr(FILE_PATH_LITERAL("--luci")))))))
      .Times(3);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, FuzzyMatching) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_launch_process(),
      Call(AllOf(
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
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  FuzzySkiaGoldMatchingAlgorithm algorithm(1, 2);
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap(), &algorithm);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, FuzzyMatchingWithIgnoredBorder) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_launch_process(),
      Call(AllOf(
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
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  FuzzySkiaGoldMatchingAlgorithm algorithm(1, 2, 3);
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap(), &algorithm);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, SobelMatching) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_launch_process(),
      Call(AllOf(
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
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  SobelSkiaGoldMatchingAlgorithm algorithm(1, 2, 3, 4);
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap(), &algorithm);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, DefaultCorpus) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(mock_launch_process(),
              Call(AllOf(Property(
                  &base::CommandLine::GetCommandLineString,
                  HasSubstr(FILE_PATH_LITERAL("--corpus=gtest-pixeltests"))))))
      .Times(1);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, ExplicitCorpus) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_launch_process(),
      Call(AllOf(Property(&base::CommandLine::GetCommandLineString,
                          HasSubstr(FILE_PATH_LITERAL("--corpus=corpus"))))))
      .Times(1);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession("corpus");
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, DefaultCodeReviewSystem) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitchASCII("gerrit-issue", "1");
  cmd_line->AppendSwitchASCII("gerrit-patchset", "2");
  cmd_line->AppendSwitchASCII("buildbucket-id", "3");

  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_launch_process(),
      Call(AllOf(Property(&base::CommandLine::GetCommandLineString,
                          HasSubstr(FILE_PATH_LITERAL("--crs=gerrit"))))))
      .Times(1);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, ExplicitCodeReviewSystem) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitchASCII("gerrit-issue", "1");
  cmd_line->AppendSwitchASCII("gerrit-patchset", "2");
  cmd_line->AppendSwitchASCII("buildbucket-id", "3");
  cmd_line->AppendSwitchASCII("code-review-system", "new-crs");

  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_launch_process(),
      Call(AllOf(Property(&base::CommandLine::GetCommandLineString,
                          HasSubstr(FILE_PATH_LITERAL("--crs=new-crs"))),
                 Property(&base::CommandLine::GetCommandLineString,
                          Not(HasSubstr(FILE_PATH_LITERAL("gerrit")))))))
      .Times(1);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, DryRunLocally) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->RemoveSwitch(switches::kTestLauncherBotMode);
  base::ScopedEnvironmentVariableOverride env_override(
      "CHROMIUM_TEST_LAUNCHER_BOT_MODE");

  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(mock_launch_process(),
              Call(AllOf(Property(&base::CommandLine::GetCommandLineString,
                                  HasSubstr(FILE_PATH_LITERAL("--dryrun"))))))
      .Times(1);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, NotDryRunOnBots) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  cmd_line->AppendSwitch(switches::kTestLauncherBotMode);

  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      mock_launch_process(),
      Call(AllOf(Property(&base::CommandLine::GetCommandLineString,
                          Not(HasSubstr(FILE_PATH_LITERAL("--dryrun")))))))
      .Times(3);
  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, ReuseSessionDefaultParameters) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());

  {  // Check that the default test environment doesn't cause a cache miss
    auto* instance = SkiaGoldPixelDiff::GetSession("corpus");
    auto* instance2 = SkiaGoldPixelDiff::GetSession("corpus");
    EXPECT_EQ(instance, instance2);
  }

  {  // Check that the default corpus doesn't cause a cache miss
    auto* instance = SkiaGoldPixelDiff::GetSession();
    auto* instance2 = SkiaGoldPixelDiff::GetSession();
    EXPECT_EQ(instance, instance2);
  }
}

TEST_F(SkiaGoldPixelDiffTest, ReuseSession) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());

  const std::string corpus = "corpus";
  const TestEnvironmentMap test_environment = {
      {TestEnvironmentKey::kSystemVersion, "value"}};

  auto* instance = SkiaGoldPixelDiff::GetSession(corpus, test_environment);
  EXPECT_EQ(instance, SkiaGoldPixelDiff::GetSession(corpus, test_environment));

  auto* instance_different_corpus = SkiaGoldPixelDiff::GetSession(
      "different-corpus-for-testing", test_environment);
  EXPECT_NE(instance, instance_different_corpus);

  auto* instance_different_map = SkiaGoldPixelDiff::GetSession(
      corpus, TestEnvironmentMap{{TestEnvironmentKey::kSystemVersion,
                                  "different-value-for-testing"}});
  EXPECT_NE(instance, instance_different_map);

  // Check that after some modifications to the session cache, we can still get
  // cache hits.
  EXPECT_EQ(instance, SkiaGoldPixelDiff::GetSession(corpus, test_environment));
  EXPECT_EQ(instance_different_corpus,
            SkiaGoldPixelDiff::GetSession("different-corpus-for-testing",
                                          test_environment));
  EXPECT_EQ(instance_different_map,
            SkiaGoldPixelDiff::GetSession(
                corpus, TestEnvironmentMap{{TestEnvironmentKey::kSystemVersion,
                                            "different-value-for-testing"}}));
}

// Check that |GetGoldenImageName| contains at least all the parts put into it.
TEST_F(SkiaGoldPixelDiffTest, GetGoldenImageName) {
  {
    const std::string name =
        SkiaGoldPixelDiff::GetGoldenImageName("Prefix", "TestName");
    EXPECT_TRUE(name.find("Prefix") != std::string::npos);
    EXPECT_TRUE(name.find("TestName") != std::string::npos);
  }

  {
    const std::string name =
        SkiaGoldPixelDiff::GetGoldenImageName("Prefix", "TestName", {"Suffix"});
    EXPECT_TRUE(name.find("Prefix") != std::string::npos);
    EXPECT_TRUE(name.find("TestName") != std::string::npos);
    EXPECT_TRUE(name.find("Suffix") != std::string::npos);
  }
}

TEST_F(SkiaGoldPixelDiffTest, GetGoldenImageNameParameterizedTest) {
  {
    const std::string name = SkiaGoldPixelDiff::GetGoldenImageName(
        "TestSuiteName", "TestName/ParamValue");
    EXPECT_TRUE(name.find("TestSuiteName") != std::string::npos);
    EXPECT_TRUE(name.find("TestName") != std::string::npos);
    EXPECT_TRUE(name.find("ParamValue") != std::string::npos);
  }

  {
    const std::string name = SkiaGoldPixelDiff::GetGoldenImageName(
        "ParamInstantiation/TestSuiteName", "TestName/ParamValue");
    EXPECT_TRUE(name.find("ParamInstantiation") != std::string::npos);
    EXPECT_TRUE(name.find("TestSuiteName") != std::string::npos);
    EXPECT_TRUE(name.find("TestName") != std::string::npos);
    EXPECT_TRUE(name.find("ParamValue") != std::string::npos);
  }

  {
    const std::string name = SkiaGoldPixelDiff::GetGoldenImageName(
        "ParamInstantiation/TestSuiteName", "TestName/ParamValue", {"Suffix"});
    EXPECT_TRUE(name.find("ParamInstantiation") != std::string::npos);
    EXPECT_TRUE(name.find("TestSuiteName") != std::string::npos);
    EXPECT_TRUE(name.find("TestName") != std::string::npos);
    EXPECT_TRUE(name.find("ParamValue") != std::string::npos);
    EXPECT_TRUE(name.find("Suffix") != std::string::npos);
  }
}

#if DCHECK_IS_ON() && defined(GTEST_HAS_DEATH_TEST)
TEST_F(SkiaGoldPixelDiffTest, MustBeUsedSequentially) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());

  auto* mock_pixel = SkiaGoldPixelDiff::GetSession();
  bool ret = mock_pixel->CompareScreenshot("test", GetTestBitmap());
  EXPECT_TRUE(ret);

  auto thread = base::Thread("NonSequentialThread");
  ASSERT_TRUE(thread.StartAndWaitForTesting());
  thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto* mock_pixel2 = SkiaGoldPixelDiff::GetSession();
        EXPECT_EQ(mock_pixel, mock_pixel2);
        EXPECT_DCHECK_DEATH_WITH(
            mock_pixel2->CompareScreenshot("test", GetTestBitmap()),
            "CalledOnValidSequence");
      }));
  thread.FlushForTesting();
}

TEST_F(SkiaGoldPixelDiffTest, GoldenImageNameCannotHaveIllegalCharacters) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());

  EXPECT_DCHECK_DEATH_WITH(
      SkiaGoldPixelDiff::GetSession()->CompareScreenshot(
          "image_name_with space", GetTestBitmap()),
      "a golden image name should not contain any space or back slash");

  EXPECT_DCHECK_DEATH_WITH(
      SkiaGoldPixelDiff::GetSession()->CompareScreenshot(
          "image_name_with/slash", GetTestBitmap()),
      "a golden image name should not contain any space or back slash");
}
#endif

}  // namespace test
}  // namespace ui

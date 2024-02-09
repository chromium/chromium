// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/view_skia_gold_pixel_diff.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::IsTrue;
using ::testing::ResultOf;
using ::testing::Return;

namespace views {

class FakeBrowserSkiaGoldPixelDiff : public ViewSkiaGoldPixelDiff {
 public:
  explicit FakeBrowserSkiaGoldPixelDiff(const std::string& screenshot_prefix)
      : ViewSkiaGoldPixelDiff(screenshot_prefix) {}

  bool GrabWindowSnapshotInternal(gfx::NativeWindow window,
                                  const gfx::Rect& snapshot_bounds,
                                  gfx::Image* image) const override {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    *image = gfx::Image::CreateFrom1xBitmap(bitmap);
    return true;
  }
};

class ViewSkiaGoldPixelDiffTest : public views::test::WidgetTest {
 public:
  using MockLaunchProcess = testing::MockFunction<
      ui::test::SkiaGoldPixelDiff::LaunchProcessCallback::RunType>;

  ViewSkiaGoldPixelDiffTest() {
    auto* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII("git-revision", "test");
  }

  ViewSkiaGoldPixelDiffTest(const ViewSkiaGoldPixelDiffTest&) = delete;
  ViewSkiaGoldPixelDiffTest& operator=(const ViewSkiaGoldPixelDiffTest&) =
      delete;

  views::View* AddChildViewToWidget(views::Widget* widget) {
    auto view_unique_ptr = std::make_unique<views::View>();
    if (widget->client_view())
      return widget->client_view()->AddChildView(std::move(view_unique_ptr));

    return widget->SetContentsView(std::move(view_unique_ptr));
  }

 protected:
  void SetUp() override {
    views::test::WidgetTest::SetUp();

    session_cache_.emplace();
    mock_launch_process_.emplace();
    auto_reset_custom_launch_process_.emplace(
        ui::test::SkiaGoldPixelDiff::OverrideLaunchProcessForTesting(
            base::BindLambdaForTesting(mock_launch_process_->AsStdFunction())));
  }

  void TearDown() override {
    views::test::WidgetTest::TearDown();

    auto_reset_custom_launch_process_.reset();
    mock_launch_process_.reset();
    session_cache_.reset();
  }

  MockLaunchProcess& mock_launch_process() {
    return mock_launch_process_.value();
  }

 private:
  std::optional<ui::test::SkiaGoldPixelDiff::ScopedSessionCacheForTesting>
      session_cache_;
  std::optional<MockLaunchProcess> mock_launch_process_;
  std::optional<
      base::AutoReset<ui::test::SkiaGoldPixelDiff::LaunchProcessCallback>>
      auto_reset_custom_launch_process_;
};

TEST_F(ViewSkiaGoldPixelDiffTest, CompareScreenshotByView) {
  EXPECT_CALL(mock_launch_process(), Call(_)).Times(AnyNumber());
  auto mock_pixel = FakeBrowserSkiaGoldPixelDiff("Prefix");
  views::Widget* widget = CreateTopLevelNativeWidget();
  views::View* child_view = AddChildViewToWidget(widget);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  constexpr char kPrefix[] = "Prefix.Demo.";
#else
  constexpr char kPrefix[] = "Prefix_Demo_";
#endif
  EXPECT_CALL(
      mock_launch_process(),
      Call(ResultOf(
          "imgtest add is called with the expected --test-name value",
          [&](const base::CommandLine& cmdline) {
            return cmdline.argv()[1] == FILE_PATH_LITERAL("imgtest") &&
                   cmdline.argv()[2] == FILE_PATH_LITERAL("add") &&
                   cmdline.GetSwitchValueASCII("test-name") ==
                       base::StrCat(
                           {kPrefix,
                            ui::test::SkiaGoldPixelDiff::GetPlatform()});
          },
          IsTrue())))
      .Times(1);
  bool ret = mock_pixel.CompareViewScreenshot("Demo", child_view);
  EXPECT_TRUE(ret);
  widget->CloseNow();
}

TEST_F(ViewSkiaGoldPixelDiffTest, BypassSkiaGoldFunctionality) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "bypass-skia-gold-functionality");

  EXPECT_CALL(mock_launch_process(), Call(_)).Times(0);
  auto mock_pixel = FakeBrowserSkiaGoldPixelDiff("Prefix");
  views::Widget* widget = CreateTopLevelNativeWidget();
  views::View* child_view = AddChildViewToWidget(widget);
  bool ret = mock_pixel.CompareViewScreenshot("Demo", child_view);
  EXPECT_TRUE(ret);
  widget->CloseNow();
}

}  // namespace views

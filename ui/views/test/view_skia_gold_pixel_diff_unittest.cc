// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/view_skia_gold_pixel_diff.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"

using ::testing::_;
using ::testing::Return;

namespace views {

class MockBrowserSkiaGoldPixelDiff : public ViewSkiaGoldPixelDiff {
 public:
  MockBrowserSkiaGoldPixelDiff() = default;
  MOCK_CONST_METHOD1(LaunchProcess, int(const base::CommandLine&));
  bool GrabWindowSnapshotInternal(gfx::NativeWindow window,
                                  const gfx::Rect& snapshot_bounds,
                                  gfx::Image* image) const override {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    *image = gfx::Image::CreateFrom1xBitmap(bitmap);
    return true;
  }
};

class MockViewSkiaGoldPixelDiffMockUpload
    : public MockBrowserSkiaGoldPixelDiff {
 public:
  MockViewSkiaGoldPixelDiffMockUpload() = default;
  MOCK_CONST_METHOD3(UploadToSkiaGoldServer,
                     bool(const base::FilePath&,
                          const std::string&,
                          const ui::test::SkiaGoldMatchingAlgorithm*));
};

class ViewSkiaGoldPixelDiffTest : public views::test::WidgetTest {
 public:
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
};

TEST_F(ViewSkiaGoldPixelDiffTest, CompareScreenshotByView) {
  MockViewSkiaGoldPixelDiffMockUpload mock_pixel;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  constexpr char kPrefix[] = "Prefix.Demo.";
#else
  constexpr char kPrefix[] = "Prefix_Demo_";
#endif

  EXPECT_CALL(mock_pixel,
              UploadToSkiaGoldServer(
                  _, kPrefix + ui::test::SkiaGoldPixelDiff::GetPlatform(), _))
      .Times(1)
      .WillOnce(Return(true));
  views::Widget* widget = CreateTopLevelNativeWidget();
  views::View* child_view = AddChildViewToWidget(widget);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareViewScreenshot("Demo", child_view);
  EXPECT_TRUE(ret);
  widget->CloseNow();
}

TEST_F(ViewSkiaGoldPixelDiffTest, BypassSkiaGoldFunctionality) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "bypass-skia-gold-functionality");

  MockBrowserSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(0);
  views::Widget* widget = CreateTopLevelNativeWidget();
  views::View* child_view = AddChildViewToWidget(widget);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareViewScreenshot("Demo", child_view);
  EXPECT_TRUE(ret);
  widget->CloseNow();
}

}  // namespace views

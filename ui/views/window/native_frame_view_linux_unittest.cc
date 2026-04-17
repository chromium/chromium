// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/native_frame_view_linux.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/linux/nav_button_provider.h"
#include "ui/linux/window_frame_provider.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/frame_buttons.h"
#include "ui/views/window/frame_view_layout_linux.h"
#include "ui/views/window/window_button_order_provider.h"

namespace views {

namespace {

constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 100;

constexpr gfx::Size kCloseButtonSize = gfx::Size(2, 3);
constexpr gfx::Size kMaximizeButtonSize = gfx::Size(5, 7);
constexpr gfx::Size kMinimizeButtonSize = gfx::Size(11, 13);

constexpr gfx::Insets kCloseButtonMargin = gfx::Insets::TLBR(17, 19, 23, 29);
constexpr gfx::Insets kMaximizeButtonMargin = gfx::Insets::TLBR(31, 37, 41, 43);
constexpr gfx::Insets kMinimizeButtonMargin = gfx::Insets::TLBR(47, 53, 59, 61);
constexpr gfx::Insets kTopAreaSpacing = gfx::Insets::TLBR(67, 71, 73, 79);
constexpr int kInterNavButtonSpacing = 83;

gfx::ImageSkia GetTestImageForSize(gfx::Size size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

class TestNavButtonProvider : public ui::NavButtonProvider {
 public:
  TestNavButtonProvider() = default;
  ~TestNavButtonProvider() override = default;

  void RedrawImages(int top_area_height, bool maximized, bool active) override {
    ++redraw_count;
  }

  gfx::ImageSkia GetImage(
      ui::NavButtonProvider::FrameButtonDisplayType type,
      ui::NavButtonProvider::ButtonState state) const override {
    switch (type) {
      case ui::NavButtonProvider::FrameButtonDisplayType::kClose:
        return GetTestImageForSize(kCloseButtonSize);
      case ui::NavButtonProvider::FrameButtonDisplayType::kMaximize:
        return GetTestImageForSize(kMaximizeButtonSize);
      case ui::NavButtonProvider::FrameButtonDisplayType::kRestore:
        return GetTestImageForSize(kMaximizeButtonSize);
      case ui::NavButtonProvider::FrameButtonDisplayType::kMinimize:
        return GetTestImageForSize(kMinimizeButtonSize);
    }
  }

  gfx::Insets GetNavButtonMargin(
      ui::NavButtonProvider::FrameButtonDisplayType type) const override {
    switch (type) {
      case ui::NavButtonProvider::FrameButtonDisplayType::kClose:
        return kCloseButtonMargin;
      case ui::NavButtonProvider::FrameButtonDisplayType::kMaximize:
      case ui::NavButtonProvider::FrameButtonDisplayType::kRestore:
        return kMaximizeButtonMargin;
      case ui::NavButtonProvider::FrameButtonDisplayType::kMinimize:
        return kMinimizeButtonMargin;
    }
  }

  gfx::Insets GetTopAreaSpacing() const override { return kTopAreaSpacing; }

  int GetNavButtonHeight(bool maximized) const override { return 0; }

  int GetInterNavButtonSpacing() const override {
    return kInterNavButtonSpacing;
  }

  int redraw_count = 0;
};

class TestFrameProvider : public ui::WindowFrameProvider {
 public:
  TestFrameProvider() = default;
  ~TestFrameProvider() override = default;

  void set_frame_thickness(const gfx::Insets& thickness) {
    frame_thickness_ = thickness;
  }

  int GetTopCornerRadiusDip() override { return 0; }
  bool IsTopFrameTranslucent() override { return false; }
  gfx::Insets GetFrameThicknessDip() override { return frame_thickness_; }
  int GetTopAreaMinHeightDip() override { return 24; }
  gfx::Insets GetTopAreaPaddingDip() override { return gfx::Insets::VH(6, 0); }
  gfx::Insets GetTopAreaBorderDip() override {
    return gfx::Insets::TLBR(0, 0, 1, 0);
  }
  void PaintWindowFrame(gfx::Canvas* canvas,
                        const gfx::Rect& rect,
                        int top_area_height,
                        bool focused,
                        const gfx::Insets& input_insets) override {}

 private:
  gfx::Insets frame_thickness_;
};

}  // namespace

class NativeFrameViewLinuxTest : public ViewsTestBase {
 public:
  NativeFrameViewLinuxTest() = default;
  NativeFrameViewLinuxTest(const NativeFrameViewLinuxTest&) = delete;
  NativeFrameViewLinuxTest& operator=(const NativeFrameViewLinuxTest&) = delete;
  ~NativeFrameViewLinuxTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    auto nav_provider = std::make_unique<TestNavButtonProvider>();
    nav_button_provider_ = nav_provider.get();
    frame_provider_ = std::make_unique<TestFrameProvider>();

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    widget_delegate_ = std::make_unique<WidgetDelegate>();
    params.delegate = widget_delegate_.get();
    params.delegate->SetCanMaximize(true);
    params.delegate->SetCanMinimize(true);
    params.delegate->SetCanResize(true);
    params.remove_standard_frame = true;
    widget_->Init(std::move(params));

    auto frame_view = std::make_unique<NativeFrameViewLinux>(
        widget_, std::move(nav_provider),
        base::BindRepeating(
            [](TestFrameProvider* fp, bool, bool) {
              return static_cast<ui::WindowFrameProvider*>(fp);
            },
            frame_provider_.get()));
    frame_view->SetSupportsClientFrameShadow(true);
    frame_view->InitViews();
    widget_->non_client_view()->SetFrameView(std::move(frame_view));

    widget_->SetBounds(gfx::Rect(0, 0, kWindowWidth, kWindowHeight));
    widget_->Show();
  }

  void TearDown() override {
    // The nav provider is owned by the frame view; clear our pointer before
    // the widget destroys it to avoid a dangling raw_ptr.
    nav_button_provider_ = nullptr;
    widget_.ExtractAsDangling()->CloseNow();
    // Reset to default trailing button order.
    WindowButtonOrderProvider::GetInstance()->SetWindowButtonOrder(
        /*leading=*/{},
        {FrameButton::kMinimize, FrameButton::kMaximize, FrameButton::kClose});
    ViewsTestBase::TearDown();
  }

 protected:
  NativeFrameViewLinux* frame_view() {
    return static_cast<NativeFrameViewLinux*>(
        widget_->non_client_view()->frame_view());
  }

  ImageButton* close_button() {
    return static_cast<ImageButton*>(frame_view()->close_button());
  }
  ImageButton* minimize_button() {
    return static_cast<ImageButton*>(frame_view()->minimize_button());
  }
  ImageButton* maximize_button() {
    return static_cast<ImageButton*>(frame_view()->maximize_button());
  }

  void SetButtonOrder(const std::vector<FrameButton>& leading,
                      const std::vector<FrameButton>& trailing) {
    WindowButtonOrderProvider::GetInstance()->SetWindowButtonOrder(leading,
                                                                   trailing);
  }

  void RunLayout() {
    frame_view()->InvalidateLayout();
    views::test::RunScheduledLayout(frame_view());
  }

  bool HasButtonCache() { return frame_view()->button_cache_.has_value(); }

  void ClearButtonCache() { frame_view()->button_cache_ = std::nullopt; }

  Widget* widget() { return widget_; }

  raw_ptr<Widget> widget_ = nullptr;
  std::unique_ptr<WidgetDelegate> widget_delegate_;
  raw_ptr<TestNavButtonProvider> nav_button_provider_ = nullptr;
  std::unique_ptr<TestFrameProvider> frame_provider_;
};

// Tests that leading window control buttons are positioned according to the
// nav button provider margins and inter-button spacing.
TEST_F(NativeFrameViewLinuxTest, NativeNavButtonPositions) {
  frame_provider_->set_frame_thickness(
      gfx::Insets(FrameViewLayoutLinux::kResizeBorder));
  SetButtonOrder(
      {FrameButton::kClose, FrameButton::kMaximize, FrameButton::kMinimize},
      {});
  RunLayout();

  const gfx::Insets insets = frame_view()->GetFrameBorderInsets();
  ASSERT_EQ(FrameViewLayoutLinux::kResizeBorder, insets.top());
  ASSERT_EQ(FrameViewLayoutLinux::kResizeBorder, insets.left());

  const int frame_top = insets.top();
  int x = insets.left() + kTopAreaSpacing.left();

  EXPECT_EQ(kCloseButtonSize, close_button()->size());
  x += kCloseButtonMargin.left();
  EXPECT_EQ(x, close_button()->x());
  EXPECT_EQ(frame_top + kCloseButtonMargin.top(), close_button()->y());

  EXPECT_EQ(kMaximizeButtonSize, maximize_button()->size());
  x += kCloseButtonSize.width() + kCloseButtonMargin.right() +
       kInterNavButtonSpacing + kMaximizeButtonMargin.left();
  EXPECT_EQ(x, maximize_button()->x());
  EXPECT_EQ(frame_top + kMaximizeButtonMargin.top(), maximize_button()->y());

  EXPECT_EQ(kMinimizeButtonSize, minimize_button()->size());
  x += kMaximizeButtonSize.width() + kMaximizeButtonMargin.right() +
       kInterNavButtonSpacing + kMinimizeButtonMargin.left();
  EXPECT_EQ(x, minimize_button()->x());
  EXPECT_EQ(frame_top + kMinimizeButtonMargin.top(), minimize_button()->y());
}

// Tests that frame border and input insets are still computed normally when
// tiled. The frame provider getter receives the tiled state so it can return
// a provider with appropriate metrics for tiled edges.
TEST_F(NativeFrameViewLinuxTest, FrameInsetsWhenTiled) {
  frame_provider_->set_frame_thickness(
      gfx::Insets(FrameViewLayoutLinux::kResizeBorder));
  frame_view()->SetTiled(true);

  EXPECT_EQ(gfx::Insets(FrameViewLayoutLinux::kResizeBorder),
            frame_view()->GetFrameBorderInsets());
  EXPECT_EQ(gfx::Insets(FrameViewLayoutLinux::kResizeBorder),
            frame_view()->GetInputInsets());
}

// Tests that GetFrameBorderInsets expands thin frame provider borders to at
// least the resize border width, ensuring resize handles remain usable.
TEST_F(NativeFrameViewLinuxTest, FrameBorderInsetsExpandToResizeBorder) {
  // A frame thinner than kResizeBorder is expanded.
  frame_provider_->set_frame_thickness(gfx::Insets(1));
  EXPECT_EQ(gfx::Insets(FrameViewLayoutLinux::kResizeBorder),
            frame_view()->GetFrameBorderInsets());

  // A frame at least as thick as kResizeBorder is not reduced.
  frame_provider_->set_frame_thickness(
      gfx::Insets(FrameViewLayoutLinux::kResizeBorder + 5));
  EXPECT_EQ(gfx::Insets(FrameViewLayoutLinux::kResizeBorder + 5),
            frame_view()->GetFrameBorderInsets());
}

// Tests that MaybeUpdateCachedFrameButtonImages() skips RedrawImages when the
// parameters haven't changed.
TEST_F(NativeFrameViewLinuxTest, ButtonImageCachingPreventsRedundantRedraws) {
  // Clear the cache to start from a known state regardless of how many
  // internal layouts SetUp may have triggered.
  ClearButtonCache();

  // First layout with cleared cache: params differ, so RedrawImages is called.
  frame_view()->InvalidateLayout();
  views::test::RunScheduledLayout(frame_view());
  ASSERT_TRUE(HasButtonCache());
  int count = nav_button_provider_->redraw_count;
  ASSERT_GT(count, 0);

  // Immediately re-running layout with the same state: cache hit, no redraw.
  frame_view()->InvalidateLayout();
  views::test::RunScheduledLayout(frame_view());
  EXPECT_EQ(count, nav_button_provider_->redraw_count);
}

// Tests that the button image cache is invalidated when the maximize state
// changes.
TEST_F(NativeFrameViewLinuxTest, ButtonImageCacheInvalidatedOnMaximizeChange) {
  int count = nav_button_provider_->redraw_count;
  ASSERT_GT(count, 0);

  // Maximizing changes the cached params, causing a cache miss and redraw.
  widget()->Maximize();
  RunLayout();
  EXPECT_EQ(count + 1, nav_button_provider_->redraw_count);

  // Restoring changes params again, causing another cache miss and redraw.
  widget()->Restore();
  RunLayout();
  EXPECT_EQ(count + 2, nav_button_provider_->redraw_count);
}

}  // namespace views

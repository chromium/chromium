// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/frame_view_linux.h"

#include <memory>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/frame_buttons.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/frame_view_layout_linux.h"
#include "ui/views/window/window_button_order_provider.h"

namespace views {

namespace {

constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;

}  // namespace

class FrameViewLinuxTest : public ViewsTestBase {
 public:
  FrameViewLinuxTest() = default;
  FrameViewLinuxTest(const FrameViewLinuxTest&) = delete;
  FrameViewLinuxTest& operator=(const FrameViewLinuxTest&) = delete;
  ~FrameViewLinuxTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    widget_delegate_ = std::make_unique<WidgetDelegate>();
    params.delegate = widget_delegate_.get();
    params.delegate->SetCanMaximize(true);
    params.delegate->SetCanMinimize(true);
    params.delegate->SetCanResize(true);
    params.delegate->SetShowTitle(true);
    params.delegate->SetTitle(u"Test Window");
    params.remove_standard_frame = true;
    widget_->Init(std::move(params));

    auto frame_view = std::make_unique<FrameViewLinux>(widget_);
    frame_view->SetSupportsClientFrameShadow(true);
    frame_view->InitViews();
    widget_->non_client_view()->SetFrameView(std::move(frame_view));

    widget_->SetBounds(gfx::Rect(0, 0, kWindowWidth, kWindowHeight));
    widget_->Show();
  }

  void TearDown() override {
    widget_.ExtractAsDangling()->CloseNow();
    WindowButtonOrderProvider::GetInstance()->SetWindowButtonOrder(
        /*leading=*/{},
        {FrameButton::kMinimize, FrameButton::kMaximize, FrameButton::kClose});
    ViewsTestBase::TearDown();
  }

 protected:
  FrameViewLinux* frame_view() {
    return static_cast<FrameViewLinux*>(
        widget_->non_client_view()->frame_view());
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

  Widget* widget() { return widget_; }
  WidgetDelegate* widget_delegate() { return widget_delegate_.get(); }

  raw_ptr<Widget> widget_ = nullptr;
  std::unique_ptr<WidgetDelegate> widget_delegate_;
};

// Tests button layout with mixed leading [close] and trailing [min, max].
// This exercises the full LayoutWindowControls path including both leading
// and trailing iteration, button ordering, visibility, and vertical centering.
TEST_F(FrameViewLinuxTest, ButtonLayout) {
  SetButtonOrder({FrameButton::kClose},
                 {FrameButton::kMinimize, FrameButton::kMaximize});
  RunLayout();

  auto* min = frame_view()->minimize_button();
  auto* max = frame_view()->maximize_button();
  auto* close = frame_view()->close_button();
  auto* restore = frame_view()->restore_button();

  EXPECT_TRUE(min->GetVisible());
  EXPECT_TRUE(max->GetVisible());
  EXPECT_TRUE(close->GetVisible());
  EXPECT_FALSE(restore->GetVisible());

  // Close on the left, min/max on the right, ordered left-to-right.
  EXPECT_LT(close->bounds().right(), kWindowWidth / 2);
  EXPECT_GT(min->x(), kWindowWidth / 2);
  EXPECT_LT(min->x(), max->x());

  // Buttons should be vertically within the titlebar.
  int top_area = frame_view()->GetBoundsForClientView().y();
  for (auto* button : {min, max, close}) {
    EXPECT_GE(button->y(), 0);
    EXPECT_LE(button->bounds().bottom(), top_area);
  }
}

// Tests that maximizing swaps maximize for restore.
TEST_F(FrameViewLinuxTest, MaximizeRevealsRestoreButton) {
  RunLayout();

  EXPECT_TRUE(frame_view()->maximize_button()->GetVisible());
  EXPECT_FALSE(frame_view()->restore_button()->GetVisible());

  widget()->Maximize();
  RunLayout();

  EXPECT_FALSE(frame_view()->maximize_button()->GetVisible());
  EXPECT_TRUE(frame_view()->restore_button()->GetVisible());
}

// Tests that CanMaximize=false hides both maximize and restore.
TEST_F(FrameViewLinuxTest, CannotMaximizeHidesButtons) {
  widget_delegate()->SetCanMaximize(false);
  RunLayout();

  EXPECT_FALSE(frame_view()->maximize_button()->GetVisible());
  EXPECT_FALSE(frame_view()->restore_button()->GetVisible());
  EXPECT_TRUE(frame_view()->minimize_button()->GetVisible());
  EXPECT_TRUE(frame_view()->close_button()->GetVisible());
}

// Tests that CanMinimize=false hides minimize.
TEST_F(FrameViewLinuxTest, CannotMinimizeHidesButton) {
  widget_delegate()->SetCanMinimize(false);
  RunLayout();

  EXPECT_FALSE(frame_view()->minimize_button()->GetVisible());
  EXPECT_TRUE(frame_view()->maximize_button()->GetVisible());
  EXPECT_TRUE(frame_view()->close_button()->GetVisible());
}

// Tests that layout with zero-size view does not crash.
TEST_F(FrameViewLinuxTest, EmptyBoundsSkipsLayout) {
  widget()->SetBounds(gfx::Rect(0, 0, 0, 0));
  RunLayout();
  // No crash is sufficient; buttons should not be positioned.
}

// Tests hit testing for caption buttons, restore button, caption area, and
// points outside the frame.
TEST_F(FrameViewLinuxTest, HitTestCaptionButtonsAndArea) {
  RunLayout();

  // Outside bounds.
  EXPECT_EQ(HTNOWHERE, frame_view()->NonClientHitTest(gfx::Point(-1, -1)));

  // Caption buttons.
  EXPECT_EQ(
      HTCLOSE,
      frame_view()->NonClientHitTest(
          frame_view()->close_button()->GetMirroredBounds().CenterPoint()));
  EXPECT_EQ(
      HTMAXBUTTON,
      frame_view()->NonClientHitTest(
          frame_view()->maximize_button()->GetMirroredBounds().CenterPoint()));
  EXPECT_EQ(
      HTMINBUTTON,
      frame_view()->NonClientHitTest(
          frame_view()->minimize_button()->GetMirroredBounds().CenterPoint()));

  // Restore button after maximize.
  widget()->Maximize();
  RunLayout();
  EXPECT_EQ(
      HTMAXBUTTON,
      frame_view()->NonClientHitTest(
          frame_view()->restore_button()->GetMirroredBounds().CenterPoint()));
  widget()->Restore();
  RunLayout();

  // Titlebar area between buttons.
  int top_area = frame_view()->GetBoundsForClientView().y();
  gfx::Insets border = frame_view()->GetFrameBorderInsets();
  gfx::Point caption_point(kWindowWidth / 2,
                           border.top() + (top_area - border.top()) / 2);
  EXPECT_EQ(HTCAPTION, frame_view()->NonClientHitTest(caption_point));
}

// Tests hit testing for resize edges and corners.
TEST_F(FrameViewLinuxTest, HitTestResizeRegions) {
  RunLayout();

  gfx::Insets border = frame_view()->GetFrameBorderInsets();

  // Edges.
  EXPECT_EQ(HTLEFT, frame_view()->NonClientHitTest(
                        gfx::Point(border.left() / 2, kWindowHeight / 2)));
  EXPECT_EQ(HTRIGHT,
            frame_view()->NonClientHitTest(gfx::Point(
                kWindowWidth - border.right() / 2, kWindowHeight / 2)));
  EXPECT_EQ(HTBOTTOM,
            frame_view()->NonClientHitTest(gfx::Point(
                kWindowWidth / 2, kWindowHeight - border.bottom() / 2)));

  // Corners.
  EXPECT_EQ(HTBOTTOMLEFT,
            frame_view()->NonClientHitTest(gfx::Point(
                border.left() / 2, kWindowHeight - border.bottom() / 2)));
  EXPECT_EQ(HTBOTTOMRIGHT, frame_view()->NonClientHitTest(gfx::Point(
                               kWindowWidth - border.right() / 2,
                               kWindowHeight - border.bottom() / 2)));
}

// Tests that the top resize strip works when shadows are disabled.
TEST_F(FrameViewLinuxTest, HitTestTopResizeNoShadow) {
  frame_view()->SetSupportsClientFrameShadow(false);
  RunLayout();

  gfx::Insets border = frame_view()->GetFrameBorderInsets();
  EXPECT_EQ(0, border.top());

  // A point near the very top of the frame should be HTTOP.
  EXPECT_EQ(HTTOP,
            frame_view()->NonClientHitTest(gfx::Point(kWindowWidth / 2, 1)));

  // A point further down in the titlebar should be HTCAPTION.
  int top_area = frame_view()->GetBoundsForClientView().y();
  EXPECT_EQ(HTCAPTION, frame_view()->NonClientHitTest(
                           gfx::Point(kWindowWidth / 2, top_area / 2)));

  // Side resize should still work.
  EXPECT_EQ(HTLEFT, frame_view()->NonClientHitTest(
                        gfx::Point(border.left() / 2, kWindowHeight / 2)));
}

// Tests that hidden buttons and disabled resize don't produce hit codes.
TEST_F(FrameViewLinuxTest, HitTestDisabledCapabilities) {
  // Hidden minimize button.
  widget_delegate()->SetCanMinimize(false);
  RunLayout();
  gfx::Point where_min_was(frame_view()->minimize_button()->bounds().x() + 1,
                           frame_view()->minimize_button()->bounds().y() + 1);
  EXPECT_NE(HTMINBUTTON, frame_view()->NonClientHitTest(where_min_was));

  // Disabled resize.
  widget_delegate()->SetCanMinimize(true);
  widget_delegate()->SetCanResize(false);
  RunLayout();
  gfx::Insets border = frame_view()->GetFrameBorderInsets();
  EXPECT_NE(HTLEFT, frame_view()->NonClientHitTest(
                        gfx::Point(border.left() / 2, kWindowHeight / 2)));
}

// Tests frame geometry in the restored state: shadow-based border insets,
// rounded top corners, non-empty input and shadow values.
TEST_F(FrameViewLinuxTest, FrameGeometryRestored) {
  gfx::Insets border = frame_view()->GetFrameBorderInsets();
  EXPECT_FALSE(border.IsEmpty());
  EXPECT_GT(border.top(), 0);

  EXPECT_FALSE(frame_view()->GetCornerRadii().IsEmpty());

  EXPECT_FALSE(frame_view()->GetInputInsets().IsEmpty());
  EXPECT_FALSE(frame_view()->GetShadowValues(true).empty());

  SkRRect clip = frame_view()->GetRestoredClipRegion();
  EXPECT_GT(clip.radii(SkRRect::kUpperLeft_Corner).x(), 0.0f);
  EXPECT_EQ(0.0f, clip.radii(SkRRect::kLowerLeft_Corner).x());
}

// Tests frame geometry when tiled: no rounded corners, no shadows,
// border still present for resize handles.
TEST_F(FrameViewLinuxTest, FrameGeometryTiled) {
  frame_view()->SetTiled(true);

  // Shadow support is still advertised when tiled; shadows are suppressed
  // at paint time via empty shadow values and zero corner radius.
  EXPECT_TRUE(frame_view()->ShouldDrawRestoredFrameShadow());
  EXPECT_TRUE(frame_view()->GetCornerRadii().IsEmpty());
  EXPECT_TRUE(frame_view()->GetShadowValues(true).empty());

  // Border insets use resize-border sizing (not the thin no-shadow border),
  // so the compositor can hide them on tiled edges via decoration insets.
  gfx::Insets border = frame_view()->GetFrameBorderInsets();
  EXPECT_FALSE(border.IsEmpty());
  EXPECT_EQ(border.left(), border.right());
  EXPECT_EQ(border.left(), border.bottom());
  EXPECT_EQ(border.left(), border.top());

  // Untiling restores shadow values and corner radius.
  frame_view()->SetTiled(false);
  EXPECT_FALSE(frame_view()->GetShadowValues(true).empty());
  EXPECT_FALSE(frame_view()->GetCornerRadii().IsEmpty());
}

// Tests frame geometry when maximized: empty borders, empty input insets,
// empty shadows.
TEST_F(FrameViewLinuxTest, FrameGeometryMaximized) {
  widget()->Maximize();

  EXPECT_TRUE(frame_view()->GetFrameBorderInsets().IsEmpty());
  EXPECT_TRUE(frame_view()->GetInputInsets().IsEmpty());
  EXPECT_TRUE(frame_view()->GetShadowValues(true).empty());
}

// Tests fallback when compositor does not support client frame shadows:
// no rounded corners, thin border on all sides with top=0, clip region
// covers full bounds.
TEST_F(FrameViewLinuxTest, FrameGeometryNoShadowSupport) {
  frame_view()->SetSupportsClientFrameShadow(false);

  EXPECT_FALSE(frame_view()->ShouldDrawRestoredFrameShadow());
  EXPECT_TRUE(frame_view()->GetCornerRadii().IsEmpty());
  EXPECT_TRUE(frame_view()->GetInputInsets().IsEmpty());

  gfx::Insets border = frame_view()->GetFrameBorderInsets();
  EXPECT_EQ(0, border.top());
  EXPECT_GT(border.left(), 0);
  EXPECT_EQ(border.left(), border.right());
  EXPECT_EQ(border.left(), border.bottom());

  // Clip region should cover full bounds, not inset by border.
  SkRRect clip = frame_view()->GetRestoredClipRegion();
  gfx::RectF clip_rect = gfx::SkRectToRectF(clip.rect());
  EXPECT_EQ(gfx::RectF(frame_view()->GetLocalBounds()), clip_rect);
}

// Tests title bounds positioning.
TEST_F(FrameViewLinuxTest, TitleBounds) {
  RunLayout();

  // Title should be centered and between the button groups.
  const gfx::Rect& title = frame_view()->title_bounds();
  auto* min = frame_view()->minimize_button();
  EXPECT_LT(title.x(), min->x());
  EXPECT_FALSE(title.IsEmpty());
  EXPECT_EQ(kWindowWidth / 2, title.x() + title.width() / 2);
}

}  // namespace views

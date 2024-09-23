// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_frame_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/footnote_container_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metrics.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/test_layout_provider.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_interactive_uitest_utils.h"
#include "ui/views/window/dialog_client_view.h"

namespace views {

namespace {

constexpr BubbleBorder::Arrow kArrow = BubbleBorder::TOP_LEFT;
constexpr int kMargin = 6;
constexpr gfx::Size kMinimumClientSize = gfx::Size(100, 200);
constexpr gfx::Size kPreferredClientSize = gfx::Size(150, 250);
constexpr gfx::Size kMaximumClientSize = gfx::Size(300, 300);

// These account for non-client areas like the title bar, footnote etc. However
// these do not take the bubble border into consideration.
gfx::Size AddAdditionalSize(gfx::Size size) {
  size.Enlarge(12, 12);
  return size;
}

class TestBubbleFrameViewWidgetDelegate : public WidgetDelegate {
 public:
  TestBubbleFrameViewWidgetDelegate() = default;
  ~TestBubbleFrameViewWidgetDelegate() override = default;

  // WidgetDelegate:
  View* GetContentsView() override {
    if (!contents_view_) {
      StaticSizedView* contents_view =
          new StaticSizedView(kPreferredClientSize);
      contents_view->set_minimum_size(kMinimumClientSize);
      contents_view->set_maximum_size(kMaximumClientSize);
      contents_view_ = contents_view;
    }
    return contents_view_;
  }
  void WindowClosing() override { contents_view_ = nullptr; }

  bool ShouldShowCloseButton() const override { return should_show_close_; }

  void SetShouldShowCloseButton(bool should_show_close) {
    should_show_close_ = should_show_close;
  }

 private:
  raw_ptr<View> contents_view_ = nullptr;  // Owned by the Widget.
  bool should_show_close_ = false;
};

class TestBubbleFrameView : public BubbleFrameView {
 public:
  explicit TestBubbleFrameView(ViewsTestBase* test_base)
      : BubbleFrameView(gfx::Insets(), gfx::Insets(kMargin)) {
    SetBubbleBorder(
        std::make_unique<BubbleBorder>(kArrow, BubbleBorder::STANDARD_SHADOW));
    widget_ = std::make_unique<Widget>();
    widget_delegate_ = std::make_unique<TestBubbleFrameViewWidgetDelegate>();
    Widget::InitParams params =
        test_base->CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                Widget::InitParams::TYPE_BUBBLE);
    params.delegate = widget_delegate_.get();
    widget_->Init(std::move(params));
  }

  TestBubbleFrameView(const TestBubbleFrameView&) = delete;
  TestBubbleFrameView& operator=(const TestBubbleFrameView&) = delete;

  ~TestBubbleFrameView() override = default;

  void SetAvailableAnchorWindowBounds(gfx::Rect bounds) {
    available_anchor_window_bounds_ = bounds;
  }

  BubbleBorder::Arrow GetBorderArrow() const {
    return bubble_border()->arrow();
  }

  gfx::Insets GetBorderInsets() const { return bubble_border()->GetInsets(); }

  // BubbleFrameView:
  Widget* GetWidget() override { return widget_.get(); }

  const Widget* GetWidget() const override { return widget_.get(); }

  gfx::Rect GetAvailableScreenBounds(const gfx::Rect& rect) const override {
    return available_bounds_;
  }

  gfx::Rect GetAvailableAnchorWindowBounds() const override {
    return available_anchor_window_bounds_;
  }

  TestBubbleFrameViewWidgetDelegate* widget_delegate() {
    return widget_delegate_.get();
  }

 private:
  const gfx::Rect available_bounds_ = gfx::Rect(0, 0, 1000, 1000);
  gfx::Rect available_anchor_window_bounds_;

  std::unique_ptr<TestBubbleFrameViewWidgetDelegate> widget_delegate_;
  std::unique_ptr<Widget> widget_;
};

}  // namespace

class BubbleFrameViewTest : public ViewsTestBase {
 public:
  BubbleFrameViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  BubbleFrameViewTest(const BubbleFrameViewTest&) = delete;
  BubbleFrameViewTest& operator=(const BubbleFrameViewTest&) = delete;

  ~BubbleFrameViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    provider_ = std::make_unique<test::TestLayoutProvider>();
  }

  test::TestLayoutProvider& provider() { return *provider_; }

 private:
  std::unique_ptr<test::TestLayoutProvider> provider_;
};

TEST_F(BubbleFrameViewTest, GetBoundsForClientView) {
  TestBubbleFrameView frame(this);
  EXPECT_EQ(kArrow, frame.GetBorderArrow());

  const gfx::Insets content_margins = frame.GetContentMargins();
  const gfx::Insets insets = frame.GetBorderInsets();
  const gfx::Rect client_view_bounds = frame.GetBoundsForClientView();
  EXPECT_EQ(insets.left() + content_margins.left(), client_view_bounds.x());
  EXPECT_EQ(insets.top() + content_margins.top(), client_view_bounds.y());
}

TEST_F(BubbleFrameViewTest, GetBoundsForClientViewWithClose) {
  TestBubbleFrameView frame(this);
  frame.widget_delegate()->SetShouldShowCloseButton(true);
  frame.ResetWindowControls();
  EXPECT_EQ(kArrow, frame.GetBorderArrow());

  const gfx::Insets content_margins = frame.GetContentMargins();
  const gfx::Insets insets = frame.GetBorderInsets();
  const int close_margin =
      frame.close_button()->height() +
      LayoutProvider::Get()->GetDistanceMetric(DISTANCE_CLOSE_BUTTON_MARGIN);
  const gfx::Rect client_view_bounds = frame.GetBoundsForClientView();
  EXPECT_EQ(insets.left() + content_margins.left(), client_view_bounds.x());
  EXPECT_EQ(insets.top() + content_margins.top() + close_margin,
            client_view_bounds.y());
}

TEST_F(BubbleFrameViewTest, RemoveFootnoteView) {
  TestBubbleFrameView frame(this);
  EXPECT_EQ(nullptr, frame.footnote_container_.get());
  auto footnote = std::make_unique<StaticSizedView>(gfx::Size(200, 200));
  View* footnote_dummy_view = footnote.get();
  frame.SetFootnoteView(std::move(footnote));
  EXPECT_EQ(footnote_dummy_view->parent(), frame.footnote_container_);
  frame.SetFootnoteView(nullptr);
  EXPECT_EQ(nullptr, frame.footnote_container_.get());
}

TEST_F(BubbleFrameViewTest,
       FootnoteContainerViewShouldMatchVisibilityOfFirstChild) {
  TestBubbleFrameView frame(this);
  std::unique_ptr<View> footnote =
      std::make_unique<StaticSizedView>(gfx::Size(200, 200));
  footnote->SetVisible(false);
  View* footnote_dummy_view = footnote.get();
  frame.SetFootnoteView(std::move(footnote));
  View* footnote_container_view = footnote_dummy_view->parent();
  EXPECT_FALSE(footnote_container_view->GetVisible());
  footnote_dummy_view->SetVisible(true);
  EXPECT_TRUE(footnote_container_view->GetVisible());
  footnote_dummy_view->SetVisible(false);
  EXPECT_FALSE(footnote_container_view->GetVisible());
}

// Tests that the arrow is mirrored as needed to better fit the screen.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBounds) {
  TestBubbleFrameView frame(this);
  gfx::Rect window_bounds;

  frame.SetBubbleBorder(
      std::make_unique<BubbleBorder>(kArrow, BubbleBorder::NO_SHADOW));

  // Test that the info bubble displays normally when it fits.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(500, 500),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 100);
  EXPECT_EQ(window_bounds.y(), 100);

  // Test bubble not fitting on left.
  frame.SetArrow(BubbleBorder::TOP_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 0, 0),       // |anchor_rect|
      BubbleBorder::Arrow::TOP_RIGHT,  // |delegate_arrow|
      gfx::Size(500, 500),             // |client_size|
      true);                           // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 100);
  EXPECT_EQ(window_bounds.y(), 100);

  // Test bubble not fitting on left or top.
  frame.SetArrow(BubbleBorder::BOTTOM_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 0, 0),          // |anchor_rect|
      BubbleBorder::Arrow::BOTTOM_RIGHT,  // |delegate_arrow|
      gfx::Size(500, 500),                // |client_size|
      true);                              // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 100);
  EXPECT_EQ(window_bounds.y(), 100);

  // Test bubble not fitting on top.
  frame.SetArrow(BubbleBorder::BOTTOM_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 0, 0),         // |anchor_rect|
      BubbleBorder::Arrow::BOTTOM_LEFT,  // |delegate_arrow|
      gfx::Size(500, 500),               // |client_size|
      true);                             // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 100);
  EXPECT_EQ(window_bounds.y(), 100);

  // Test bubble not fitting on top and right.
  frame.SetArrow(BubbleBorder::BOTTOM_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 100, 0, 0),         // |anchor_rect|
      BubbleBorder::Arrow::BOTTOM_LEFT,  // |delegate_arrow|
      gfx::Size(500, 500),               // |client_size|
      true);                             // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.right(), 900);
  EXPECT_EQ(window_bounds.y(), 100);

  // Test bubble not fitting on right.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 100, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(500, 500),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.right(), 900);
  EXPECT_EQ(window_bounds.y(), 100);

  // Test bubble not fitting on bottom and right.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 900, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(500, 500),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.right(), 900);
  EXPECT_EQ(window_bounds.bottom(), 900);

  // Test bubble not fitting at the bottom.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(500, 500),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT, frame.GetBorderArrow());
  // The window should be right aligned with the anchor_rect.
  EXPECT_EQ(window_bounds.x(), 100);
  EXPECT_EQ(window_bounds.bottom(), 900);

  // Test bubble not fitting at the bottom and left.
  frame.SetArrow(BubbleBorder::TOP_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 0, 0),       // |anchor_rect|
      BubbleBorder::Arrow::TOP_RIGHT,  // |delegate_arrow|
      gfx::Size(500, 500),             // |client_size|
      true);                           // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT, frame.GetBorderArrow());
  // The window should be right aligned with the anchor_rect.
  EXPECT_EQ(window_bounds.x(), 100);
  EXPECT_EQ(window_bounds.bottom(), 900);
}

// Tests that the arrow is not moved when the info-bubble does not fit the
// screen but moving it would make matter worse.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsMirroringFails) {
  TestBubbleFrameView frame(this);
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  frame.GetUpdatedWindowBounds(
      gfx::Rect(400, 100, 50, 50),    // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(500, 700),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
}

TEST_F(BubbleFrameViewTest, TestMirroringForCenteredArrow) {
  TestBubbleFrameView frame(this);

  // Test bubble not fitting above the anchor.
  frame.SetArrow(BubbleBorder::BOTTOM_CENTER);
  frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),         // |anchor_rect|
      BubbleBorder::Arrow::BOTTOM_CENTER,  // |delegate_arrow|
      gfx::Size(500, 700),                 // |client_size|
      true);                               // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_CENTER, frame.GetBorderArrow());

  // Test bubble not fitting below the anchor.
  frame.SetArrow(BubbleBorder::TOP_CENTER);
  frame.GetUpdatedWindowBounds(
      gfx::Rect(300, 800, 50, 50),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_CENTER,  // |delegate_arrow|
      gfx::Size(500, 200),              // |client_size|
      true);                            // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.GetBorderArrow());

  // Test bubble not fitting to the right of the anchor.
  frame.SetArrow(BubbleBorder::LEFT_CENTER);
  frame.GetUpdatedWindowBounds(
      gfx::Rect(800, 300, 50, 50),       // |anchor_rect|
      BubbleBorder::Arrow::LEFT_CENTER,  // |delegate_arrow|
      gfx::Size(200, 500),               // |client_size|
      true);                             // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER, frame.GetBorderArrow());

  // Test bubble not fitting to the left of the anchor.
  frame.SetArrow(BubbleBorder::RIGHT_CENTER);
  frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 300, 50, 50),        // |anchor_rect|
      BubbleBorder::Arrow::RIGHT_CENTER,  // |delegate_arrow|
      gfx::Size(500, 500),                // |client_size|
      true);                              // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::LEFT_CENTER, frame.GetBorderArrow());
}

// Test that the arrow will not be mirrored when
// |adjust_to_fit_available_bounds| is false.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsDontTryMirror) {
  TestBubbleFrameView frame(this);
  frame.SetBubbleBorder(std::make_unique<BubbleBorder>(
      BubbleBorder::TOP_RIGHT, BubbleBorder::NO_SHADOW));
  gfx::Rect window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 0, 0),       // |anchor_rect|
      BubbleBorder::Arrow::TOP_RIGHT,  // |delegate_arrow|
      gfx::Size(500, 500),             // |client_size|
      false);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.GetBorderArrow());
  // The coordinates should be pointing to anchor_rect from TOP_RIGHT.
  EXPECT_EQ(window_bounds.right(), 100);
  EXPECT_EQ(window_bounds.y(), 900);
}

// Test that the center arrow is moved as needed to fit the screen.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsCenterArrows) {
  TestBubbleFrameView frame(this);
  gfx::Rect window_bounds;

  frame.SetBubbleBorder(
      std::make_unique<BubbleBorder>(kArrow, BubbleBorder::NO_SHADOW));

  // Some of these tests may go away once --secondary-ui-md becomes the
  // default. Under Material Design mode, the BubbleBorder doesn't support all
  // "arrow" positions. If this changes, then the tests should be updated or
  // added for MD mode.

  // Test that the bubble displays normally when it fits.
  frame.SetArrow(BubbleBorder::BOTTOM_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(500, 900, 50, 50),         // |anchor_rect|
      BubbleBorder::Arrow::BOTTOM_CENTER,  // |delegate_arrow|
      gfx::Size(500, 500),                 // |client_size|
      true);                               // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x() + window_bounds.width() / 2, 525);

  frame.SetArrow(BubbleBorder::LEFT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 400, 50, 50),       // |anchor_rect|
      BubbleBorder::Arrow::LEFT_CENTER,  // |delegate_arrow|
      gfx::Size(500, 500),               // |client_size|
      true);                             // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::LEFT_CENTER, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.y() + window_bounds.height() / 2, 425);

  frame.SetArrow(BubbleBorder::RIGHT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 400, 50, 50),        // |anchor_rect|
      BubbleBorder::Arrow::RIGHT_CENTER,  // |delegate_arrow|
      gfx::Size(500, 500),                // |client_size|
      true);                              // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.y() + window_bounds.height() / 2, 425);

  // Test bubble not fitting left screen edge.
  frame.SetArrow(BubbleBorder::BOTTOM_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 50, 50),         // |anchor_rect|
      BubbleBorder::Arrow::BOTTOM_CENTER,  // |delegate_arrow|
      gfx::Size(500, 500),                 // |client_size|
      true);                               // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 0);

  // Test bubble not fitting right screen edge.
  frame.SetArrow(BubbleBorder::BOTTOM_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 900, 50, 50),         // |anchor_rect|
      BubbleBorder::Arrow::BOTTOM_CENTER,  // |delegate_arrow|
      gfx::Size(500, 500),                 // |client_size|
      true);                               // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.right(), 1000);
}

// Tests that the arrow is mirrored as needed to better fit the anchor window's
// bounds.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsForBubbleWithAnchorWindow) {
  TestBubbleFrameView frame(this);
  frame.SetAvailableAnchorWindowBounds(gfx::Rect(100, 100, 500, 500));
  gfx::Rect window_bounds;

  frame.SetBubbleBorder(
      std::make_unique<BubbleBorder>(kArrow, BubbleBorder::NO_SHADOW));

  // Test that the bubble displays normally when it fits.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(200, 200, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 200);
  EXPECT_EQ(window_bounds.y(), 200);

  // Test bubble not fitting on left for anchor window displays left aligned
  // with the left side of the anchor rect.
  frame.SetArrow(BubbleBorder::TOP_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(200, 200, 0, 0),       // |anchor_rect|
      BubbleBorder::Arrow::TOP_RIGHT,  // |delegate_arrow|
      gfx::Size(250, 250),             // |client_size|
      true);                           // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 200);
  EXPECT_EQ(window_bounds.y(), 200);

  // Test bubble not fitting on left or top displays left and top aligned
  // with the left and bottom sides of the anchor rect.
  frame.SetArrow(BubbleBorder::BOTTOM_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(200, 200, 0, 0),          // |anchor_rect|
      BubbleBorder::Arrow::BOTTOM_RIGHT,  // |delegate_arrow|
      gfx::Size(250, 250),                // |client_size|
      true);                              // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 200);
  EXPECT_EQ(window_bounds.y(), 200);

  // Test bubble not fitting on top displays top aligned with the bottom side of
  // the anchor rect.
  frame.SetArrow(BubbleBorder::BOTTOM_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(200, 200, 0, 0),         // |anchor_rect|
      BubbleBorder::Arrow::BOTTOM_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),               // |client_size|
      true);                             // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 200);
  EXPECT_EQ(window_bounds.y(), 200);

  // Test bubble not fitting on top and right displays right and top aligned
  // with the right and bottom sides of the anchor rect.
  frame.SetArrow(BubbleBorder::BOTTOM_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(500, 200, 0, 0),         // |anchor_rect|
      BubbleBorder::Arrow::BOTTOM_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),               // |client_size|
      true);                             // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.right(), 500);
  EXPECT_EQ(window_bounds.y(), 200);

  // Test bubble not fitting on right display in line with the right edge of
  // the anchor rect.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(500, 200, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.right(), 500);
  EXPECT_EQ(window_bounds.y(), 200);

  // Test bubble not fitting on bottom and right displays in line with the right
  // edge of the anchor rect and the bottom in line with the top of the anchor
  // rect.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(500, 500, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.right(), 500);
  EXPECT_EQ(window_bounds.bottom(), 500);

  // Test bubble not fitting at the bottom displays line with the top of the
  // anchor rect.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(200, 500, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 200);
  EXPECT_EQ(window_bounds.bottom(), 500);

  // Test bubble not fitting at the bottom and left displays right aligned with
  // the anchor rect and the bottom in line with the top of the anchor rect.
  frame.SetArrow(BubbleBorder::TOP_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(200, 500, 0, 0),       // |anchor_rect|
      BubbleBorder::Arrow::TOP_RIGHT,  // |delegate_arrow|
      gfx::Size(250, 250),             // |client_size|
      true);                           // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 200);
  EXPECT_EQ(window_bounds.bottom(), 500);
}

// Tests that the arrow is mirrored as needed to better fit the screen.
TEST_F(BubbleFrameViewTest,
       GetUpdatedWindowBoundsForBubbleWithAnchorWindowExitingScreen) {
  TestBubbleFrameView frame(this);
  gfx::Rect window_bounds;

  frame.SetBubbleBorder(
      std::make_unique<BubbleBorder>(kArrow, BubbleBorder::NO_SHADOW));

  // Test bubble fitting anchor window and not fitting screen on right.
  //     ________________________
  //    |screen _________________|__________
  //    |      |anchor window ___|___       |
  //    |      |             |bubble |      |
  //    |      |             |_______|      |
  //    |      |_________________|__________|
  //    |________________________|
  frame.SetAvailableAnchorWindowBounds(gfx::Rect(700, 200, 400, 400));
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(800, 300, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.GetBorderArrow());
  // The window should be right aligned with the anchor_rect.
  EXPECT_EQ(window_bounds.right(), 800);
  EXPECT_EQ(window_bounds.y(), 300);

  // Test bubble fitting anchor window and not fitting screen on right and
  // bottom.
  //     ________________________
  //    |screen                  |
  //    |       _________________|__________
  //    |      |anchor window ___|___       |
  //    |______|_____________|bubble |      |
  //           |             |_______|      |
  //           |____________________________|
  frame.SetAvailableAnchorWindowBounds(gfx::Rect(700, 700, 400, 400));
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(800, 800, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT, frame.GetBorderArrow());
  // The window should be right aligned with the anchor_rect.
  EXPECT_EQ(window_bounds.right(), 800);
  EXPECT_EQ(window_bounds.bottom(), 800);

  // Test bubble not fitting anchor window on bottom and not fitting screen on
  // right.
  //     ________________________
  //    |screen _________________|__________
  //    |      |anchor window    |          |
  //    |      |              ___|___       |
  //    |      |_____________|bubble |______|
  //    |                    |_______|
  //    |________________________|
  frame.SetAvailableAnchorWindowBounds(gfx::Rect(700, 200, 400, 400));
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(800, 500, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT, frame.GetBorderArrow());
  // The window should be right aligned with the anchor_rect.
  EXPECT_EQ(window_bounds.right(), 800);
  EXPECT_EQ(window_bounds.bottom(), 500);
}

// Tests that bubbles with `use_anchor_window_bounds_` set to false will not
// apply an offset to try to make them fit inside the anchor window bounds.
TEST_F(BubbleFrameViewTest, BubbleNotUsingAnchorWindowBounds) {
  TestBubbleFrameView frame(this);
  gfx::Rect window_bounds;

  frame.SetBubbleBorder(
      std::make_unique<BubbleBorder>(kArrow, BubbleBorder::NO_SHADOW));

  // Test bubble not fitting anchor window on bottom and not fitting screen on
  // right.
  //     ________________________
  //    |screen _________________|__________
  //    |      |anchor window    |          |
  //    |      |              ___|___       |
  //    |      |_____________|bubble |______|
  //    |                    |_______|
  //    |________________________|

  frame.SetAvailableAnchorWindowBounds(gfx::Rect(700, 200, 400, 400));
  frame.set_use_anchor_window_bounds(false);
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(800, 500, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|

  // The window should be right aligned with the anchor_rect.
  EXPECT_EQ(window_bounds.right(), 800);

  // Bubble will not try to fit inside the anchor window.
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.GetBorderArrow());
  EXPECT_GT(window_bounds.bottom(), 500);
}

// Tests that the arrow is mirrored as needed to better fit the anchor window's
// bounds.
TEST_F(BubbleFrameViewTest, MirroringNotStickyForGetUpdatedWindowBounds) {
  TestBubbleFrameView frame(this);
  gfx::Rect window_bounds;

  frame.SetBubbleBorder(
      std::make_unique<BubbleBorder>(kArrow, BubbleBorder::NO_SHADOW));

  // Test bubble fitting anchor window and not fitting screen on right.
  frame.SetAvailableAnchorWindowBounds(gfx::Rect(700, 200, 400, 400));
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(800, 300, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.GetBorderArrow());
  // The window should be right aligned with the anchor_rect.
  EXPECT_EQ(window_bounds.right(), 800);
  EXPECT_EQ(window_bounds.y(), 300);

  // Test that the bubble mirrors again if it can fit on screen with its
  // original anchor.
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(700, 300, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  // The window should be right aligned with the anchor_rect.
  EXPECT_EQ(window_bounds.x(), 700);
  EXPECT_EQ(window_bounds.y(), 300);
}

// Tests that the arrow is offset as needed to better fit the window.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsForBubbleSetToOffset) {
  TestBubbleFrameView frame(this);
  frame.SetAvailableAnchorWindowBounds(gfx::Rect(100, 100, 500, 500));
  frame.SetPreferredArrowAdjustment(
      BubbleFrameView::PreferredArrowAdjustment::kOffset);
  gfx::Rect window_bounds;

  frame.SetBubbleBorder(
      std::make_unique<BubbleBorder>(kArrow, BubbleBorder::NO_SHADOW));

  // Test that the bubble displays normally when it fits.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(200, 200, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 200);

  // Test bubble not fitting left window edge displayed against left window
  // edge.
  frame.SetArrow(BubbleBorder::TOP_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(200, 200, 0, 0),       // |anchor_rect|
      BubbleBorder::Arrow::TOP_RIGHT,  // |delegate_arrow|
      gfx::Size(250, 250),             // |client_size|
      true);                           // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 100);

  // Test bubble not fitting right window edge displays against the right edge
  // of the anchor window.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(500, 200, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.right(), 600);

  // Test bubble fitting anchor window and not fitting screen on right displays
  // against the right edge of the screen.
  frame.SetAvailableAnchorWindowBounds(gfx::Rect(800, 300, 500, 500));
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 500, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(250, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.right(), 1000);
}

// Tests that the arrow is offset as needed to better fit the window for
// windows larger than the available bounds.
TEST_F(BubbleFrameViewTest,
       GetUpdatedWindowBoundsForBubbleSetToOffsetLargerThanAvailableBounds) {
  TestBubbleFrameView frame(this);
  frame.SetAvailableAnchorWindowBounds(gfx::Rect(200, 200, 500, 500));
  frame.SetPreferredArrowAdjustment(
      BubbleFrameView::PreferredArrowAdjustment::kOffset);
  gfx::Rect window_bounds;

  frame.SetBubbleBorder(
      std::make_unique<BubbleBorder>(kArrow, BubbleBorder::NO_SHADOW));

  // Test that the bubble exiting right side of anchor window displays against
  // left edge of anchor window bounds if larger than anchor window.
  frame.SetArrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(300, 300, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::TOP_LEFT,  // |delegate_arrow|
      gfx::Size(600, 250),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.x(), 200);

  // Test that the bubble exiting left side of anchor window displays against
  // right edge of anchor window bounds if larger than anchor window.
  frame.SetArrow(BubbleBorder::TOP_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(300, 300, 0, 0),       // |anchor_rect|
      BubbleBorder::Arrow::TOP_RIGHT,  // |delegate_arrow|
      gfx::Size(600, 250),             // |client_size|
      true);                           // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.GetBorderArrow());
  // Check that the right edge of the bubble equals the right edge of the
  // anchor window.
  EXPECT_EQ(window_bounds.right(), 700);

  // Test that the bubble exiting bottom side of anchor window displays against
  // top edge of anchor window bounds if larger than anchor window.
  frame.SetArrow(BubbleBorder::LEFT_TOP);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(400, 400, 0, 0),      // |anchor_rect|
      BubbleBorder::Arrow::LEFT_TOP,  // |delegate_arrow|
      gfx::Size(250, 600),            // |client_size|
      true);                          // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::LEFT_TOP, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.y(), 200);

  // Test that the bubble exiting top side of anchor window displays against
  // bottom edge of anchor window bounds if larger than anchor window.
  frame.SetArrow(BubbleBorder::LEFT_BOTTOM);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(300, 300, 0, 0),         // |anchor_rect|
      BubbleBorder::Arrow::LEFT_BOTTOM,  // |delegate_arrow|
      gfx::Size(250, 600),               // |client_size|
      true);                             // |adjust_to_fit_available_bounds|
  EXPECT_EQ(BubbleBorder::LEFT_BOTTOM, frame.GetBorderArrow());
  EXPECT_EQ(window_bounds.bottom(), 700);
}

TEST_F(BubbleFrameViewTest, GetPreferredSize) {
  // Test border/insets.
  TestBubbleFrameView frame(this);
  gfx::Rect preferred_rect(frame.GetPreferredSize({}));
  // Expect that a border has been added to the preferred size.
  preferred_rect.Inset(frame.GetBorderInsets());

  gfx::Size expected_size = AddAdditionalSize(kPreferredClientSize);
  EXPECT_EQ(expected_size, preferred_rect.size());
}

TEST_F(BubbleFrameViewTest, GetPreferredSizeWithFootnote) {
  // Test footnote view: adding a footnote should increase the preferred size,
  // but only when the footnote is visible.
  TestBubbleFrameView frame(this);

  constexpr int kFootnoteHeight = 20;
  const gfx::Size no_footnote_size = frame.GetPreferredSize({});
  std::unique_ptr<View> footnote =
      std::make_unique<StaticSizedView>(gfx::Size(10, kFootnoteHeight));
  footnote->SetVisible(false);
  View* footnote_dummy_view = footnote.get();
  frame.SetFootnoteView(std::move(footnote));
  EXPECT_EQ(no_footnote_size, frame.GetPreferredSize({}));  // No change.

  footnote_dummy_view->SetVisible(true);
  gfx::Size with_footnote_size = no_footnote_size;
  constexpr int kFootnoteTopBorderThickness = 1;
  with_footnote_size.Enlarge(0, kFootnoteHeight + kFootnoteTopBorderThickness +
                                    frame.GetContentMargins().height());
  EXPECT_EQ(with_footnote_size, frame.GetPreferredSize({}));

  footnote_dummy_view->SetVisible(false);
  EXPECT_EQ(no_footnote_size, frame.GetPreferredSize({}));
}

TEST_F(BubbleFrameViewTest, GetMinimumSize) {
  TestBubbleFrameView frame(this);
  gfx::Rect minimum_rect(frame.GetMinimumSize());
  // Expect that a border has been added to the minimum size.
  minimum_rect.Inset(frame.GetBorderInsets());

  gfx::Size expected_size = AddAdditionalSize(kMinimumClientSize);
  EXPECT_EQ(expected_size, minimum_rect.size());
}

TEST_F(BubbleFrameViewTest, GetMaximumSize) {
  TestBubbleFrameView frame(this);
  gfx::Rect maximum_rect(frame.GetMaximumSize());
#if BUILDFLAG(IS_WIN)
  // On Windows, GetMaximumSize causes problems with DWM, so it should just be 0
  // (unlimited). See http://crbug.com/506206.
  EXPECT_EQ(gfx::Size(), maximum_rect.size());
#else
  maximum_rect.Inset(frame.GetBorderInsets());

  // Should ignore the contents view's maximum size and use the preferred size.
  gfx::Size expected_size = AddAdditionalSize(kPreferredClientSize);
  EXPECT_EQ(expected_size, maximum_rect.size());
#endif
}

TEST_F(BubbleFrameViewTest, LayoutWithHeader) {
  // Test header view: adding a header should increase the preferred size, but
  // only when the header is visible.
  TestBubbleFrameView frame(this);

  constexpr int kHeaderHeight = 20;
  const gfx::Size no_header_size = frame.GetPreferredSize({});
  std::unique_ptr<View> header =
      std::make_unique<StaticSizedView>(gfx::Size(10, kHeaderHeight));
  header->SetVisible(false);
  View* header_raw_pointer = header.get();
  frame.SetHeaderView(std::move(header));
  EXPECT_EQ(no_header_size, frame.GetPreferredSize({}));  // No change.

  header_raw_pointer->SetVisible(true);
  gfx::Size with_header_size = no_header_size;
  with_header_size.Enlarge(0, kHeaderHeight);
  EXPECT_EQ(with_header_size, frame.GetPreferredSize({}));

  header_raw_pointer->SetVisible(false);
  EXPECT_EQ(no_header_size, frame.GetPreferredSize({}));
}

TEST_F(BubbleFrameViewTest, LayoutWithHeaderAndCloseButton) {
  // Test header view with close button: the client bounds should be positioned
  // below the header and close button, whichever is further down.
  TestBubbleFrameView frame(this);
  frame.widget_delegate()->SetShouldShowCloseButton(true);

  const int close_margin =
      frame.close_button()->height() +
      LayoutProvider::Get()->GetDistanceMetric(DISTANCE_CLOSE_BUTTON_MARGIN);
  const gfx::Insets content_margins = frame.GetContentMargins();
  const gfx::Insets insets = frame.GetBorderInsets();

  // Header is smaller than close button + margin, expect bounds to be below the
  // close button.
  frame.SetHeaderView(
      std::make_unique<StaticSizedView>(gfx::Size(10, close_margin - 1)));

  gfx::Rect client_view_bounds = frame.GetBoundsForClientView();
  EXPECT_EQ(insets.top() + content_margins.top() + close_margin,
            client_view_bounds.y());

  // Header is larger than close button + margin, expect bounds to be below the
  // header view.
  frame.SetHeaderView(
      std::make_unique<StaticSizedView>(gfx::Size(10, close_margin + 1)));

  client_view_bounds = frame.GetBoundsForClientView();
  EXPECT_EQ(insets.top() + content_margins.top() + close_margin + 1,
            client_view_bounds.y());
}

TEST_F(BubbleFrameViewTest, MetadataTest) {
  TestBubbleFrameView frame(this);
  TestBubbleFrameView* frame_pointer = &frame;
  test::TestViewMetadata(frame_pointer);
}

namespace {

class TestBubbleDialogDelegateView : public BubbleDialogDelegateView {
  METADATA_HEADER(TestBubbleDialogDelegateView, BubbleDialogDelegateView)

 public:
  TestBubbleDialogDelegateView()
      : BubbleDialogDelegateView(nullptr,
                                 BubbleBorder::NONE,
                                 BubbleBorder::NO_SHADOW,
                                 true) {
    SetAnchorRect(gfx::Rect());
    DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  }

  TestBubbleDialogDelegateView(const TestBubbleDialogDelegateView&) = delete;
  TestBubbleDialogDelegateView& operator=(const TestBubbleDialogDelegateView&) =
      delete;

  ~TestBubbleDialogDelegateView() override = default;

  void ChangeTitle(const std::u16string& title) {
    title_ = title;
    // UpdateWindowTitle() will lead to an invalidation if the title's string or
    // visibility changes.
    GetWidget()->UpdateWindowTitle();
  }

  void ChangeSubtitle(const std::u16string& subtitle) {
    subtitle_ = subtitle;
    GetBubbleFrameView()->UpdateSubtitle();
  }

  // BubbleDialogDelegateView:
  using BubbleDialogDelegateView::SetAnchorView;
  std::u16string GetWindowTitle() const override { return title_; }
  std::u16string GetSubtitle() const override { return subtitle_; }
  bool ShouldShowWindowTitle() const override { return !title_.empty(); }
  bool ShouldShowCloseButton() const override { return should_show_close_; }
  void SetShouldShowCloseButton(bool should_show_close) {
    should_show_close_ = should_show_close;
  }

  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override {
    return gfx::Size(200, 200);
  }

  BubbleFrameView* GetBubbleFrameView() const {
    return static_cast<BubbleFrameView*>(
        GetWidget()->non_client_view()->frame_view());
  }

 private:
  std::u16string title_;
  std::u16string subtitle_;
  bool should_show_close_ = false;
};

BEGIN_METADATA(TestBubbleDialogDelegateView)
END_METADATA

class TestAnchor {
 public:
  explicit TestAnchor(Widget::InitParams params) {
    params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
    widget_.Init(std::move(params));
    widget_.Show();
  }

  TestAnchor(const TestAnchor&) = delete;
  TestAnchor& operator=(const TestAnchor&) = delete;

  Widget& widget() { return widget_; }

 private:
  Widget widget_;
};

// BubbleDialogDelegate with no margins to test width snapping.
class TestWidthSnapDelegate : public TestBubbleDialogDelegateView {
  METADATA_HEADER(TestWidthSnapDelegate, TestBubbleDialogDelegateView)

 public:
  TestWidthSnapDelegate(TestAnchor* anchor, bool should_snap) {
    DialogDelegate::SetButtons(
        should_snap ? static_cast<int>(ui::mojom::DialogButton::kOk)
                    : static_cast<int>(ui::mojom::DialogButton::kNone));
    SetAnchorView(anchor->widget().GetContentsView());
    set_margins(gfx::Insets());
    BubbleDialogDelegateView::CreateBubble(this);
    GetWidget()->Show();
  }

  TestWidthSnapDelegate(const TestWidthSnapDelegate&) = delete;
  TestWidthSnapDelegate& operator=(const TestWidthSnapDelegate&) = delete;
};

BEGIN_METADATA(TestWidthSnapDelegate)
END_METADATA

}  // namespace

// This test ensures that if the installed LayoutProvider snaps dialog widths,
// BubbleFrameView correctly sizes itself to that width.
TEST_F(BubbleFrameViewTest, WidthSnaps) {
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));

  {
    TestWidthSnapDelegate* const delegate =
        new TestWidthSnapDelegate(&anchor, true);
    WidgetAutoclosePtr widget(delegate->GetWidget());
    EXPECT_EQ(delegate->GetPreferredSize({}).width(),
              delegate->GetWidget()->GetWindowBoundsInScreen().width());
  }

  constexpr int kTestWidth = 300;
  provider().SetSnappedDialogWidth(kTestWidth);

  {
    TestWidthSnapDelegate* const delegate =
        new TestWidthSnapDelegate(&anchor, true);
    WidgetAutoclosePtr widget(delegate->GetWidget());
    // The Widget's snapped width should exactly match the width returned by the
    // LayoutProvider.
    EXPECT_EQ(kTestWidth,
              delegate->GetWidget()->GetWindowBoundsInScreen().width());
  }

  {
    // If the DialogDelegate asks not to snap, it should not snap.
    TestWidthSnapDelegate* const delegate =
        new TestWidthSnapDelegate(&anchor, false);
    WidgetAutoclosePtr widget(delegate->GetWidget());
    EXPECT_EQ(delegate->GetPreferredSize({}).width(),
              delegate->GetWidget()->GetWindowBoundsInScreen().width());
  }
}

// Tests edge cases when the frame's title view starts to wrap text. This is to
// ensure that the calculations BubbleFrameView does to determine the Widget
// size for a given client view are consistent with the eventual size that the
// client view takes after layout.
TEST_F(BubbleFrameViewTest, LayoutEdgeCases) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());

  Widget* bubble =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  bubble->Show();

  // Even though the bubble has default margins, the dialog view should have
  // been given its preferred size.
  EXPECT_FALSE(delegate->margins().IsEmpty());
  EXPECT_EQ(delegate->size(), delegate->GetPreferredSize({}));

  // Starting with a short title.
  std::u16string title(1, 'i');
  delegate->ChangeTitle(title);
  const int min_bubble_height = bubble->GetWindowBoundsInScreen().height();
  EXPECT_LT(delegate->GetPreferredSize({}).height(), min_bubble_height);

  // Grow the title incrementally until word wrap is required. There should
  // never be a point where the BubbleFrameView over- or under-estimates the
  // size required for the title. If it did, it would automatically resizes the
  // Widget based on autosize, requiring the subsequent Layout() to fill the
  // remaining client area with something other than |delegate|'s preferred
  // size.
  while (bubble->GetWindowBoundsInScreen().height() == min_bubble_height) {
    title += ' ';
    title += 'i';
    delegate->ChangeTitle(title);
    EXPECT_EQ(delegate->GetPreferredSize({}), delegate->size()) << title;
  }

  // Sanity check that something interesting happened. The bubble should have
  // grown by "a line" for the wrapped title, and the title should have reached
  // a length that would have likely caused word wrap. A typical result would be
  // a +17-20 change in height and title length of 53 characters.
  const int two_line_height = bubble->GetWindowBoundsInScreen().height();
  EXPECT_LT(12, two_line_height - min_bubble_height);
  EXPECT_GT(25, two_line_height - min_bubble_height);
  EXPECT_LT(30u, title.size());
  EXPECT_GT(80u, title.size());

  // Now add dialog snapping.
  provider().SetSnappedDialogWidth(300);
  // Only test::TestLayoutProvider has a setter(SetSnappedDialogWidth()), it
  // can not invalidate the exact view. So it only actively InvalidateLayout()
  // after SetSnappedDialogWidth() in the test code.
  delegate->InvalidateLayout();

  // Height should go back to |min_bubble_height| since the window is wider:
  // word wrapping should no longer happen.
  EXPECT_EQ(min_bubble_height, bubble->GetWindowBoundsInScreen().height());
  EXPECT_EQ(300, bubble->GetWindowBoundsInScreen().width());

  // Now we are allowed to diverge from the client view width, but not height.
  EXPECT_EQ(delegate->GetPreferredSize({}).height(), delegate->height());
  EXPECT_LT(delegate->GetPreferredSize({}).width(), delegate->width());
  EXPECT_GT(300, delegate->width());  // Greater, since there are margins.

  const gfx::Size snapped_size = delegate->size();
  const size_t old_title_size = title.size();

  // Grow the title again with width snapping until word wrapping occurs.
  while (bubble->GetWindowBoundsInScreen().height() == min_bubble_height) {
    title += ' ';
    title += 'i';
    delegate->ChangeTitle(title);
    EXPECT_EQ(snapped_size, delegate->size()) << title;
  }
  // Change to the height should have been the same as before. Title should
  // have grown about 50%.
  EXPECT_EQ(two_line_height, bubble->GetWindowBoundsInScreen().height());
  EXPECT_LT(15u, title.size() - old_title_size);
  EXPECT_GT(40u, title.size() - old_title_size);

  // When |anchor| goes out of scope it should take |bubble| with it.
}

// Tests edge cases when the frame's title view starts to wrap text when a
// header view is set. This is to ensure the title leaves enough space for the
// close button when there is a header or not.
TEST_F(BubbleFrameViewTest, LayoutEdgeCasesWithHeader) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());
  delegate->SetShouldShowCloseButton(true);
  Widget* bubble =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  bubble->Show();

  BubbleFrameView* frame = delegate->GetBubbleFrameView();
  const int close_margin =
      frame->close_button()->height() +
      LayoutProvider::Get()->GetDistanceMetric(DISTANCE_CLOSE_BUTTON_MARGIN);

  // Set a header view that is 1 dip smaller than the close button.
  frame->SetHeaderView(
      std::make_unique<StaticSizedView>(gfx::Size(10, close_margin - 1)));

  // Starting with a short title.
  std::u16string title(1, 'i');
  delegate->ChangeTitle(title);
  const int min_bubble_height = bubble->GetWindowBoundsInScreen().height();

  // Grow the title incrementally until word wrap is required.
  while (bubble->GetWindowBoundsInScreen().height() == min_bubble_height) {
    title += ' ';
    title += 'i';
    delegate->ChangeTitle(title);
  }

  // Sanity check that something interesting happened. The bubble should have
  // grown by "a line" for the wrapped title.
  const int two_line_height = bubble->GetWindowBoundsInScreen().height();
  EXPECT_LT(12, two_line_height - min_bubble_height);
  EXPECT_GT(25, two_line_height - min_bubble_height);

  // Now grow the header view to be the same size as the close button. This
  // should allow the text to fit into a single line again as it is now allowed
  // to grow below the close button.
  frame->SetHeaderView(
      std::make_unique<StaticSizedView>(gfx::Size(10, close_margin)));

  // Height should go back to |min_bubble_height| + 1 since the window is wider:
  // word wrapping should no longer happen, the 1 dip extra height is caused by
  // growing the header view.
  EXPECT_EQ(min_bubble_height + 1, bubble->GetWindowBoundsInScreen().height());

  // When |anchor| goes out of scope it should take |bubble| with it.
}

// Layout tests with Subtitle label.
// This will test adding a Subtitle and wrap-around case for Subtitle.
TEST_F(BubbleFrameViewTest, LayoutSubtitleEdgeCases) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());
  delegate->SetSubtitleAllowCharacterBreak(true);

  Widget* bubble =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  bubble->Show();

  // Even though the bubble has default margins, the dialog view should have
  // been given its preferred size.
  EXPECT_FALSE(delegate->margins().IsEmpty());
  EXPECT_EQ(delegate->size(), delegate->GetPreferredSize({}));

  // Add title to bubble frame view.
  delegate->ChangeTitle(u"This is a title");

  int min_bubble_height = bubble->GetWindowBoundsInScreen().height();
  EXPECT_LT(delegate->GetPreferredSize({}).height(), min_bubble_height);

  // Add a short subtitle to guarantee a one-line addition.
  // Line height can vary depending on the platform so check
  // boundary where the height diff is between 12 and 18.
  // (12 < single_line_height < 18)
  std::u16string subtitle(1, 'j');
  delegate->ChangeSubtitle(subtitle);
  int line_height_diff =
      bubble->GetWindowBoundsInScreen().height() - min_bubble_height;
  EXPECT_GT(line_height_diff, 12);
  EXPECT_LT(line_height_diff, 18);

  // Set the new min bubble height with a Subtitle added.
  min_bubble_height = bubble->GetWindowBoundsInScreen().height();
  // Grow the subtitle incrementally until a wrap is required.
  while (bubble->GetWindowBoundsInScreen().height() == min_bubble_height) {
    // Use a single character to check that character breaks are enabled.
    subtitle += u"j";
    delegate->ChangeSubtitle(subtitle);
  }

  // Subtitle wrap should have increased by one line.
  line_height_diff =
      bubble->GetWindowBoundsInScreen().height() - min_bubble_height;
  EXPECT_GT(line_height_diff, 12);
  EXPECT_LT(line_height_diff, 18);

  // Turn off character breaks and confirm the height has returned to the single
  // line height.
  delegate->SetSubtitleAllowCharacterBreak(false);
  EXPECT_EQ(bubble->GetWindowBoundsInScreen().height(), min_bubble_height);
}

TEST_F(BubbleFrameViewTest, LayoutWithIcon) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());
  SkBitmap bitmap;
  bitmap.allocN32Pixels(20, 80);
  bitmap.eraseColor(SK_ColorYELLOW);
  delegate->SetIcon(ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFrom1xBitmap(bitmap)));
  delegate->SetShowIcon(true);

  Widget* widget =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  widget->Show();

  delegate->ChangeTitle(u"test title");
  BubbleFrameView* frame = delegate->GetBubbleFrameView();
  View* icon = frame->title_icon_;
  View* title = frame->title_container_;

  // There should be equal amounts of space on the left and right of the icon.
  EXPECT_EQ(icon->x() * 2 + icon->width(), title->x());

  // The title should be vertically centered relative to the icon.
  EXPECT_LT(title->height(), icon->height());
  const int title_offset_y = (icon->height() - title->height()) / 2;
  EXPECT_EQ(icon->y() + title_offset_y, title->y());
}

// Test the size of the bubble allows a |gfx::NO_ELIDE| title to fit, even if
// there is no content.
TEST_F(BubbleFrameViewTest, NoElideTitle) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());

  // Make sure the client area size doesn't interfere with the final size.
  delegate->SetPreferredSize(gfx::Size());

  Widget* bubble =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  bubble->Show();

  // Before changing the title, get the base width of the bubble when there's no
  // title or content in it.
  const int empty_bubble_width = bubble->GetClientAreaBoundsInScreen().width();
  std::u16string title = u"This is a title string";
  delegate->ChangeTitle(title);
  Label* title_label =
      static_cast<Label*>(delegate->GetBubbleFrameView()->title());

  // Sanity check: Title labels default to multiline and elide tail. Either of
  // which result in the Layout system making the title and resulting dialog
  // very narrow.
  EXPECT_EQ(gfx::ELIDE_TAIL, title_label->GetElideBehavior());
  EXPECT_TRUE(title_label->GetMultiLine());
  EXPECT_GT(empty_bubble_width, title_label->width());
  EXPECT_EQ(empty_bubble_width, bubble->GetClientAreaBoundsInScreen().width());

  // Set the title to a non-eliding label.
  title_label->SetElideBehavior(gfx::NO_ELIDE);
  title_label->SetMultiLine(false);

  // The title/bubble should now be bigger than in multiline tail-eliding mode.
  EXPECT_LT(empty_bubble_width, title_label->width());
  EXPECT_LT(empty_bubble_width, bubble->GetClientAreaBoundsInScreen().width());

  // Make sure the bubble is wide enough to fit the title's full size. Frame
  // sizing is done off the title label's minimum size. But since that label is
  // set to NO_ELIDE, the minimum size should match the preferred size.
  EXPECT_GE(bubble->GetClientAreaBoundsInScreen().width(),
            title_label->GetPreferredSize({title_label->width(), {}}).width());
  EXPECT_LE(title_label->GetPreferredSize({title_label->width(), {}}).width(),
            title_label->width());
  EXPECT_EQ(title, title_label->GetDisplayTextForTesting());
}

TEST_F(BubbleFrameViewTest, LabelWithHeadingLevel) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());
  delegate->SetSubtitleAllowCharacterBreak(true);

  Widget* bubble =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  bubble->Show();

  std::u16string title = u"This is a title string";
  delegate->ChangeTitle(title);
  Label* title_label =
      static_cast<Label*>(delegate->GetBubbleFrameView()->title());
  EXPECT_EQ(title, title_label->GetDisplayTextForTesting());

  ui::AXNodeData node_data;
  title_label->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(
      node_data.HasIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel));
  EXPECT_EQ(
      node_data.GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel),
      1);
}

// Ensures that clicks are ignored for short time after view has been shown.
TEST_F(BubbleFrameViewTest, IgnorePossiblyUnintendedClicksClose) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());
  delegate->SetShouldShowCloseButton(true);
  Widget* bubble =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  bubble->Show();

  BubbleFrameView* frame = delegate->GetBubbleFrameView();
  test::ButtonTestApi(frame->close_)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_NONE, ui::EF_NONE));
  EXPECT_FALSE(bubble->IsClosed());

  test::ButtonTestApi(frame->close_)
      .NotifyClick(ui::MouseEvent(
          ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
          ui::EventTimeForNow() + base::Milliseconds(GetDoubleClickInterval()),
          ui::EF_NONE, ui::EF_NONE));
  EXPECT_TRUE(bubble->IsClosed());
}

// Ensures that clicks are ignored for short time after view has been shown.
TEST_F(BubbleFrameViewTest, IgnorePossiblyUnintendedClicksMinimize) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());
  delegate->SetCanMinimize(true);
  Widget* bubble =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  bubble->Show();

  BubbleFrameView* frame = delegate->GetBubbleFrameView();
  test::ButtonTestApi(frame->minimize_)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_NONE, ui::EF_NONE));
  EXPECT_FALSE(bubble->IsClosed());

  views::test::PropertyWaiter minimize_waiter(
      base::BindRepeating(&Widget::IsMinimized, base::Unretained(bubble)),
      true);
  test::ButtonTestApi(frame->minimize_)
      .NotifyClick(ui::MouseEvent(
          ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
          ui::EventTimeForNow() + base::Milliseconds(GetDoubleClickInterval()),
          ui::EF_NONE, ui::EF_NONE));
  EXPECT_TRUE(minimize_waiter.Wait());
  EXPECT_TRUE(bubble->IsMinimized());
}

// Ensures that clicks are ignored for short time after anchor view bounds
// changed.
TEST_F(BubbleFrameViewTest, IgnorePossiblyUnintendedClicksAnchorBoundsChanged) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());
  delegate->SetCanMinimize(true);
  Widget* bubble =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  bubble->Show();
  ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), ui::EF_NONE,
                             ui::EF_NONE);
  BubbleFrameView* frame = delegate->GetBubbleFrameView();
  test::ButtonTestApi(frame->minimize_).NotifyClick(mouse_event);
  auto* widget = delegate->GetWidget();
  auto* dialog = delegate->GetDialogClientView();
  auto* ok_button = dialog->ok_button();
  test::ButtonTestApi(ok_button).NotifyClick(mouse_event);
  EXPECT_FALSE(bubble->IsMinimized());
  EXPECT_FALSE(widget->IsClosed());

  task_environment()->FastForwardBy(
      base::Milliseconds(GetDoubleClickInterval()));
  anchor.widget().SetBounds(gfx::Rect(10, 10, 100, 100));

  ui::MouseEvent mouse_event_1(ui::EventType::kMousePressed, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(), ui::EF_NONE,
                               ui::EF_NONE);
  test::ButtonTestApi(ok_button).NotifyClick(mouse_event_1);
  test::ButtonTestApi(frame->minimize_).NotifyClick(mouse_event_1);
  EXPECT_FALSE(widget->IsClosed());
  EXPECT_FALSE(bubble->IsMinimized());

  test::ButtonTestApi(ok_button).NotifyClick(ui::MouseEvent(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      ui::EventTimeForNow() + base::Milliseconds(GetDoubleClickInterval()),
      ui::EF_NONE, ui::EF_NONE));
  EXPECT_TRUE(widget->IsClosed());
}

// Ensures that layout is correct when the progress indicator is visible.
TEST_F(BubbleFrameViewTest, LayoutWithProgressIndicator) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());
  Widget* bubble =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  bubble->Show();

  BubbleFrameView* frame = delegate->GetBubbleFrameView();
  frame->SetProgress(/*infinite animation*/ -1);
  View* progress_indicator = frame->progress_indicator_;

  // Ensures the progress indicator is visible and takes full widget width.
  EXPECT_TRUE(progress_indicator->GetVisible());
  EXPECT_EQ(progress_indicator->x(), 0);
  EXPECT_EQ(progress_indicator->y(), 0);
  EXPECT_EQ(progress_indicator->width(),
            bubble->GetWindowBoundsInScreen().width());
}

// Close should be the next element after minimize.
TEST_F(BubbleFrameViewTest, MinimizeBeforeClose) {
  auto delegate_unique = std::make_unique<TestBubbleDialogDelegateView>();
  TestBubbleDialogDelegateView* const delegate = delegate_unique.get();
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate->SetAnchorView(anchor.widget().GetContentsView());
  delegate->SetShouldShowCloseButton(true);
  delegate->SetCanMinimize(true);
  Widget* bubble =
      BubbleDialogDelegateView::CreateBubble(std::move(delegate_unique));
  bubble->Show();

  auto minimze_iter = std::find_if(
      delegate->GetBubbleFrameView()->children().begin(),
      delegate->GetBubbleFrameView()->children().end(), [](views::View* child) {
        return child->GetProperty(views::kElementIdentifierKey) ==
               BubbleFrameView::kMinimizeButtonElementId;
      });
  ASSERT_NE(minimze_iter, delegate->GetBubbleFrameView()->children().end());
  EXPECT_EQ((*++minimze_iter)->GetProperty(views::kElementIdentifierKey),
            BubbleFrameView::kCloseButtonElementId);
}

}  // namespace views

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/controls/scrollbar/scroll_bar_views.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace views {

namespace {

// The Scrollbar controller. This is the widget that should do the real
// scrolling of contents.
class TestScrollBarController : public ScrollBarController {
 public:
  virtual ~TestScrollBarController() = default;

  void ScrollToPosition(ScrollBar* source, int position) override {
    last_source = source;
    last_position = position;
  }

  int GetScrollIncrement(ScrollBar* source,
                         bool is_page,
                         bool is_positive) override {
    last_source = source;
    last_is_page = is_page;
    last_is_positive = is_positive;

    if (is_page)
      return 20;
    return 10;
  }

  // We save the last values in order to assert the correctness of the scroll
  // operation.
  raw_ptr<ScrollBar> last_source = nullptr;
  bool last_is_positive = false;
  bool last_is_page = false;
  int last_position = 0;
};

// This container is used to forward gesture events to the scrollbar for
// testing fling and other gestures.
class TestScrollbarContainer : public View {
 public:
  TestScrollbarContainer() = default;
  ~TestScrollbarContainer() override = default;
  TestScrollbarContainer(const TestScrollbarContainer&) = delete;
  TestScrollbarContainer& operator=(const TestScrollbarContainer&) = delete;

  void OnGestureEvent(ui::GestureEvent* event) override {
    children()[0]->OnGestureEvent(event);
  }
};

}  // namespace

class ScrollBarViewsTest : public ViewsTestBase {
 public:
  static constexpr int kScrollBarID = 123;

  ScrollBarViewsTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    controller_ = std::make_unique<TestScrollBarController>();

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(0, 0, 100, 300);
    widget_->Init(std::move(params));
    widget_->Show();
    auto* container =
        widget_->SetContentsView(std::make_unique<TestScrollbarContainer>());

    auto* bar = container->AddChildView(std::make_unique<ScrollBarViews>());
    bar->SetBounds(0, 0, 100, 100);
    bar->Update(100, 1000, 0);
    bar->set_controller(controller_.get());
    bar->SetID(kScrollBarID);

    track_size_ = scrollbar()->GetTrackBounds().width();
  }

  void TearDown() override {
    scrollbar()->set_controller(nullptr);
    controller_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  ScrollBar* scrollbar() {
    return static_cast<ScrollBar*>(
        widget_->GetContentsView()->GetViewByID(kScrollBarID));
  }

  UniqueWidgetPtr widget_;


  // Keep track of the size of the track. This is how we can tell when we
  // scroll to the middle.
  int track_size_;

  std::unique_ptr<TestScrollBarController> controller_;
};

// Verify properties are accessible via metadata.
TEST_F(ScrollBarViewsTest, MetaDataTest) {
  test::TestViewMetadata(scrollbar());
}

TEST_F(ScrollBarViewsTest, Scrolling) {
  EXPECT_EQ(0, scrollbar()->GetPosition());
  EXPECT_EQ(900, scrollbar()->GetMaxPosition());
  EXPECT_EQ(0, scrollbar()->GetMinPosition());

  // Scroll to middle.
  scrollbar()->ScrollToThumbPosition(track_size_ / 2, true);
  EXPECT_EQ(450, controller_->last_position);
  EXPECT_EQ(scrollbar(), controller_->last_source);

  // Scroll to the end.
  scrollbar()->ScrollToThumbPosition(track_size_, true);
  EXPECT_EQ(900, controller_->last_position);

  // Overscroll. Last position should be the maximum position.
  scrollbar()->ScrollToThumbPosition(track_size_ + 100, true);
  EXPECT_EQ(900, controller_->last_position);

  // Underscroll. Last position should be the minimum position.
  scrollbar()->ScrollToThumbPosition(-10, false);
  EXPECT_EQ(0, controller_->last_position);

  // Test the different fixed scrolling amounts. Generally used by buttons,
  // or click on track.
  scrollbar()->ScrollToThumbPosition(0, false);
  scrollbar()->ScrollByAmount(ScrollBar::ScrollAmount::kNextLine);
  EXPECT_EQ(10, controller_->last_position);

  scrollbar()->ScrollByAmount(ScrollBar::ScrollAmount::kPrevLine);
  EXPECT_EQ(0, controller_->last_position);

  scrollbar()->ScrollByAmount(ScrollBar::ScrollAmount::kNextPage);
  EXPECT_EQ(20, controller_->last_position);

  scrollbar()->ScrollByAmount(ScrollBar::ScrollAmount::kPrevPage);
  EXPECT_EQ(0, controller_->last_position);
}

TEST_F(ScrollBarViewsTest, ScrollBarFitsToBottom) {
  scrollbar()->Update(100, 1999, 0);
  EXPECT_EQ(0, scrollbar()->GetPosition());
  EXPECT_EQ(1899, scrollbar()->GetMaxPosition());
  EXPECT_EQ(0, scrollbar()->GetMinPosition());

  // Scroll to the midpoint of the document.
  scrollbar()->Update(100, 1999, 950);
  EXPECT_EQ((scrollbar()->GetTrackBounds().width() -
             scrollbar()->GetThumbLengthForTesting()) /
                2,
            scrollbar()->GetPosition());

  // Scroll to the end of the document.
  scrollbar()->Update(100, 1999, 1899);
  EXPECT_EQ(scrollbar()->GetTrackBounds().width() -
                scrollbar()->GetThumbLengthForTesting(),
            scrollbar()->GetPosition());
}

TEST_F(ScrollBarViewsTest, ScrollToEndAfterShrinkAndExpand) {
  // Scroll to the end of the content.
  scrollbar()->Update(100, 1001, 0);
  EXPECT_TRUE(scrollbar()->ScrollByContentsOffset(-1));
  // Shrink and then re-expand the content.
  scrollbar()->Update(100, 1000, 0);
  scrollbar()->Update(100, 1001, 0);
  // Ensure the scrollbar allows scrolling to the end.
  EXPECT_TRUE(scrollbar()->ScrollByContentsOffset(-1));
}

TEST_F(ScrollBarViewsTest, ThumbFullLengthOfTrack) {
  // Shrink content so that it fits within the viewport.
  scrollbar()->Update(100, 10, 0);
  EXPECT_EQ(scrollbar()->GetTrackBounds().width(),
            scrollbar()->GetThumbLengthForTesting());
  // Emulate a click on the full size scroll bar.
  scrollbar()->ScrollToThumbPosition(0, false);
  EXPECT_EQ(0, scrollbar()->GetPosition());
  // Emulate a key down.
  scrollbar()->ScrollByAmount(ScrollBar::ScrollAmount::kNextLine);
  EXPECT_EQ(0, scrollbar()->GetPosition());

  // Expand content so that it fits *exactly* within the viewport.
  scrollbar()->Update(100, 100, 0);
  EXPECT_EQ(scrollbar()->GetTrackBounds().width(),
            scrollbar()->GetThumbLengthForTesting());
  // Emulate a click on the full size scroll bar.
  scrollbar()->ScrollToThumbPosition(0, false);
  EXPECT_EQ(0, scrollbar()->GetPosition());
  // Emulate a key down.
  scrollbar()->ScrollByAmount(ScrollBar::ScrollAmount::kNextLine);
  EXPECT_EQ(0, scrollbar()->GetPosition());
  // Emulate a programmatic scroll to a non-zero offset. See crbug.com/1447967.
  scrollbar()->Update(100, 100, 10);
  EXPECT_EQ(scrollbar()->GetTrackBounds().width(),
            scrollbar()->GetThumbLengthForTesting());
}

TEST_F(ScrollBarViewsTest, AccessibleRole) {
  ui::AXNodeData data;
  scrollbar()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kScrollBar);
  EXPECT_EQ(scrollbar()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kScrollBar);

  data = ui::AXNodeData();
  scrollbar()->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  scrollbar()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(scrollbar()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kButton);
}

#if !BUILDFLAG(IS_MAC)
TEST_F(ScrollBarViewsTest, RightClickOpensMenu) {
  EXPECT_EQ(nullptr, scrollbar()->menu_model_);
  EXPECT_EQ(nullptr, scrollbar()->menu_runner_);
  scrollbar()->set_context_menu_controller(scrollbar());
  // Disabled on Mac because Mac's native menu is synchronous.
  scrollbar()->ShowContextMenu(scrollbar()->GetBoundsInScreen().CenterPoint(),
                               ui::MENU_SOURCE_MOUSE);
  EXPECT_NE(nullptr, scrollbar()->menu_model_);
  EXPECT_NE(nullptr, scrollbar()->menu_runner_);
}

TEST_F(ScrollBarViewsTest, TestPageScrollingByPress) {
  ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
  EXPECT_EQ(0, scrollbar()->GetPosition());
  generator.MoveMouseTo(
      scrollbar()->GetThumb()->GetBoundsInScreen().right_center() +
      gfx::Vector2d(4, 0));
  generator.ClickLeftButton();
  generator.ClickLeftButton();
  EXPECT_GT(scrollbar()->GetPosition(), 0);
}

TEST_F(ScrollBarViewsTest, DragThumbScrollsContent) {
  ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
  EXPECT_EQ(0, scrollbar()->GetPosition());
  generator.MoveMouseTo(
      scrollbar()->GetThumb()->GetBoundsInScreen().CenterPoint());
  generator.PressLeftButton();
  generator.MoveMouseBy(15, 0);
  EXPECT_GE(scrollbar()->GetPosition(), 10);

  // Dragging the mouse somewhat outside the thumb maintains scroll.
  generator.MoveMouseBy(0, 100);
  EXPECT_GE(scrollbar()->GetPosition(), 10);

  // Dragging the mouse far outside the thumb snaps back to the initial
  // scroll position.
  generator.MoveMouseBy(0, 100);
  EXPECT_EQ(0, scrollbar()->GetPosition());
}

TEST_F(ScrollBarViewsTest, DragThumbScrollsContentWhenSnapBackDisabled) {
  scrollbar()->GetThumb()->SetSnapBackOnDragOutside(false);
  ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
  EXPECT_EQ(0, scrollbar()->GetPosition());
  generator.MoveMouseTo(
      scrollbar()->GetThumb()->GetBoundsInScreen().CenterPoint());
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 0);
  EXPECT_GT(scrollbar()->GetPosition(), 0);
  // Move the mouse far down, outside the thumb.
  generator.MoveMouseBy(0, 200);
  // Position does not snap back to zero.
  EXPECT_GT(scrollbar()->GetPosition(), 0);
}

TEST_F(ScrollBarViewsTest, FlingGestureScrollsView) {
  constexpr int kNumScrollSteps = 100;
  constexpr int kScrollVelocity = 10;
  ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
  EXPECT_EQ(0, scrollbar()->GetPosition());
  const gfx::Point start_pos =
      widget_->GetContentsView()->GetBoundsInScreen().CenterPoint();
  const gfx::Point end_pos = start_pos + gfx::Vector2d(-100, 0);
  generator.GestureScrollSequence(
      start_pos, end_pos,
      generator.CalculateScrollDurationForFlingVelocity(
          start_pos, end_pos, kScrollVelocity, kNumScrollSteps),
      kNumScrollSteps);
  // Just make sure the view scrolled
  EXPECT_GT(scrollbar()->GetPosition(), 0);
}
#endif

}  // namespace views

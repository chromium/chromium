// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/controls/scrollbar/scroll_bar_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

// The Scrollbar controller. This is the widget that should do the real
// scrolling of contents.
class TestScrollBarController : public views::ScrollBarController {
 public:
  virtual ~TestScrollBarController() = default;

  void ScrollToPosition(views::ScrollBar* source, int position) override {
    last_source = source;
    last_position = position;
  }

  int GetScrollIncrement(views::ScrollBar* source,
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
  views::ScrollBar* last_source;
  bool last_is_positive;
  bool last_is_page;
  int last_position;
};

}  // namespace

namespace views {

class ScrollBarViewsTest : public ViewsTestBase {
 public:
  ScrollBarViewsTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    controller_ = std::make_unique<TestScrollBarController>();

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(0, 0, 100, 100);
    widget_->Init(std::move(params));
    View* container = new View();
    widget_->SetContentsView(container);

    scrollbar_ = new ScrollBarViews(true);
    scrollbar_->SetBounds(0, 0, 100, 100);
    scrollbar_->Update(100, 1000, 0);
    scrollbar_->set_controller(controller_.get());
    container->AddChildView(scrollbar_);

    track_size_ = scrollbar_->GetTrackBounds().width();
  }

  void TearDown() override {
    widget_->Close();
    ViewsTestBase::TearDown();
  }

 protected:
  Widget* widget_ = nullptr;

  // This is the Views scrollbar.
  ScrollBar* scrollbar_ = nullptr;

  // Keep track of the size of the track. This is how we can tell when we
  // scroll to the middle.
  int track_size_;

  std::unique_ptr<TestScrollBarController> controller_;
};

TEST_F(ScrollBarViewsTest, Scrolling) {
  EXPECT_EQ(0, scrollbar_->GetPosition());
  EXPECT_EQ(900, scrollbar_->GetMaxPosition());
  EXPECT_EQ(0, scrollbar_->GetMinPosition());

  // Scroll to middle.
  scrollbar_->ScrollToThumbPosition(track_size_ / 2, true);
  EXPECT_EQ(450, controller_->last_position);
  EXPECT_EQ(scrollbar_, controller_->last_source);

  // Scroll to the end.
  scrollbar_->ScrollToThumbPosition(track_size_, true);
  EXPECT_EQ(900, controller_->last_position);

  // Overscroll. Last position should be the maximum position.
  scrollbar_->ScrollToThumbPosition(track_size_ + 100, true);
  EXPECT_EQ(900, controller_->last_position);

  // Underscroll. Last position should be the minimum position.
  scrollbar_->ScrollToThumbPosition(-10, false);
  EXPECT_EQ(0, controller_->last_position);

  // Test the different fixed scrolling amounts. Generally used by buttons,
  // or click on track.
  scrollbar_->ScrollToThumbPosition(0, false);
  scrollbar_->ScrollByAmount(ScrollBar::ScrollAmount::kNextLine);
  EXPECT_EQ(10, controller_->last_position);

  scrollbar_->ScrollByAmount(ScrollBar::ScrollAmount::kPrevLine);
  EXPECT_EQ(0, controller_->last_position);

  scrollbar_->ScrollByAmount(ScrollBar::ScrollAmount::kNextPage);
  EXPECT_EQ(20, controller_->last_position);

  scrollbar_->ScrollByAmount(ScrollBar::ScrollAmount::kPrevPage);
  EXPECT_EQ(0, controller_->last_position);
}

TEST_F(ScrollBarViewsTest, ScrollBarFitsToBottom) {
  scrollbar_->Update(100, 1999, 0);
  EXPECT_EQ(0, scrollbar_->GetPosition());
  EXPECT_EQ(1899, scrollbar_->GetMaxPosition());
  EXPECT_EQ(0, scrollbar_->GetMinPosition());

  // Scroll to the midpoint of the document.
  scrollbar_->Update(100, 1999, 950);
  EXPECT_EQ((scrollbar_->GetTrackBounds().width() -
             scrollbar_->GetThumbSizeForTesting()) /
                2,
            scrollbar_->GetPosition());

  // Scroll to the end of the document.
  scrollbar_->Update(100, 1999, 1899);
  EXPECT_EQ(scrollbar_->GetTrackBounds().width() -
                scrollbar_->GetThumbSizeForTesting(),
            scrollbar_->GetPosition());
}

TEST_F(ScrollBarViewsTest, ScrollToEndAfterShrinkAndExpand) {
  // Scroll to the end of the content.
  scrollbar_->Update(100, 1001, 0);
  EXPECT_TRUE(scrollbar_->ScrollByContentsOffset(-1));
  // Shrink and then re-expand the content.
  scrollbar_->Update(100, 1000, 0);
  scrollbar_->Update(100, 1001, 0);
  // Ensure the scrollbar allows scrolling to the end.
  EXPECT_TRUE(scrollbar_->ScrollByContentsOffset(-1));
}

TEST_F(ScrollBarViewsTest, ThumbFullLengthOfTrack) {
  // Shrink content so that it fits within the viewport.
  scrollbar_->Update(100, 10, 0);
  EXPECT_EQ(scrollbar_->GetTrackBounds().width(),
            scrollbar_->GetThumbSizeForTesting());
  // Emulate a click on the full size scroll bar.
  scrollbar_->ScrollToThumbPosition(0, false);
  EXPECT_EQ(0, scrollbar_->GetPosition());
  // Emulate a key down.
  scrollbar_->ScrollByAmount(ScrollBar::ScrollAmount::kNextLine);
  EXPECT_EQ(0, scrollbar_->GetPosition());

  // Expand content so that it fits *exactly* within the viewport.
  scrollbar_->Update(100, 100, 0);
  EXPECT_EQ(scrollbar_->GetTrackBounds().width(),
            scrollbar_->GetThumbSizeForTesting());
  // Emulate a click on the full size scroll bar.
  scrollbar_->ScrollToThumbPosition(0, false);
  EXPECT_EQ(0, scrollbar_->GetPosition());
  // Emulate a key down.
  scrollbar_->ScrollByAmount(ScrollBar::ScrollAmount::kNextLine);
  EXPECT_EQ(0, scrollbar_->GetPosition());
}

}  // namespace views

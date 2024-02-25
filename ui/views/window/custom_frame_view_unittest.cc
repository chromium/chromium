// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/custom_frame_view.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/window_button_order_provider.h"

namespace views {

class CustomFrameViewTest : public ViewsTestBase {
 public:
  CustomFrameViewTest() = default;

  CustomFrameViewTest(const CustomFrameViewTest&) = delete;
  CustomFrameViewTest& operator=(const CustomFrameViewTest&) = delete;

  ~CustomFrameViewTest() override = default;

  CustomFrameView* custom_frame_view() {
    return static_cast<CustomFrameView*>(
        widget()->non_client_view()->frame_view());
  }

  Widget* widget() { return widget_; }

  // ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  const std::vector<views::FrameButton>& leading_buttons() {
    return WindowButtonOrderProvider::GetInstance()->leading_buttons();
  }

  const std::vector<views::FrameButton>& trailing_buttons() {
    return WindowButtonOrderProvider::GetInstance()->trailing_buttons();
  }

  ImageButton* minimize_button() {
    return custom_frame_view()->minimize_button_;
  }

  ImageButton* maximize_button() {
    return custom_frame_view()->maximize_button_;
  }

  ImageButton* restore_button() { return custom_frame_view()->restore_button_; }

  ImageButton* close_button() { return custom_frame_view()->close_button_; }

  gfx::Rect title_bounds() { return custom_frame_view()->title_bounds_; }

  void SetWindowButtonOrder(
      const std::vector<views::FrameButton> leading_buttons,
      const std::vector<views::FrameButton> trailing_buttons);

 private:
  std::unique_ptr<WidgetDelegate> widget_delegate_;
  raw_ptr<Widget> widget_ = nullptr;
};

void CustomFrameViewTest::SetUp() {
  ViewsTestBase::SetUp();

  widget_ = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  widget_delegate_ = std::make_unique<WidgetDelegate>();
  params.delegate = widget_delegate_.get();
  params.delegate->SetCanMaximize(true);
  params.delegate->SetCanMinimize(true);
  params.remove_standard_frame = true;
  widget_->Init(std::move(params));
  widget_->non_client_view()->SetFrameView(
      std::make_unique<CustomFrameView>(widget_));
}

void CustomFrameViewTest::TearDown() {
  widget_.ExtractAsDangling()->CloseNow();

  ViewsTestBase::TearDown();
}

void CustomFrameViewTest::SetWindowButtonOrder(
    const std::vector<views::FrameButton> leading_buttons,
    const std::vector<views::FrameButton> trailing_buttons) {
  WindowButtonOrderProvider::GetInstance()->SetWindowButtonOrder(
      leading_buttons, trailing_buttons);
}

// Tests that there is a default button ordering before initialization causes
// a configuration file check.
TEST_F(CustomFrameViewTest, DefaultButtons) {
  const std::vector<views::FrameButton>& trailing = trailing_buttons();
  EXPECT_EQ(trailing.size(), 3u);
  EXPECT_TRUE(leading_buttons().empty());
  EXPECT_EQ(trailing[0], views::FrameButton::kMinimize);
  EXPECT_EQ(trailing[1], views::FrameButton::kMaximize);
  EXPECT_EQ(trailing[2], views::FrameButton::kClose);
}

// Tests that layout places the buttons in order, that the restore button is
// hidden and the buttons are placed after the title.
TEST_F(CustomFrameViewTest, DefaultButtonLayout) {
  widget()->SetBounds(gfx::Rect(0, 0, 300, 100));
  widget()->Show();

  EXPECT_LT(minimize_button()->x(), maximize_button()->x());
  EXPECT_LT(maximize_button()->x(), close_button()->x());
  EXPECT_FALSE(restore_button()->GetVisible());

  EXPECT_GT(minimize_button()->x(),
            title_bounds().x() + title_bounds().width());
}

// Tests that setting the buttons to leading places them before the title.
TEST_F(CustomFrameViewTest, LeadingButtonLayout) {
  std::vector<views::FrameButton> leading;
  leading.push_back(views::FrameButton::kClose);
  leading.push_back(views::FrameButton::kMinimize);
  leading.push_back(views::FrameButton::kMaximize);

  std::vector<views::FrameButton> trailing;

  SetWindowButtonOrder(leading, trailing);

  widget()->SetBounds(gfx::Rect(0, 0, 300, 100));
  widget()->Show();
  EXPECT_LT(close_button()->x(), minimize_button()->x());
  EXPECT_LT(minimize_button()->x(), maximize_button()->x());
  EXPECT_FALSE(restore_button()->GetVisible());
  EXPECT_LT(maximize_button()->x() + maximize_button()->width(),
            title_bounds().x());
}

// Tests that layouts occurring while maximized swap the maximize button for the
// restore button
TEST_F(CustomFrameViewTest, MaximizeRevealsRestoreButton) {
  widget()->SetBounds(gfx::Rect(0, 0, 300, 100));
  widget()->Show();

  ASSERT_FALSE(restore_button()->GetVisible());
  ASSERT_TRUE(maximize_button()->GetVisible());

  widget()->Maximize();
  // Just calling Maximize() doesn't invlidate the layout immediately.
  custom_frame_view()->InvalidateLayout();
  views::test::RunScheduledLayout(custom_frame_view());

#if BUILDFLAG(IS_MAC)
  // Restore buttons do not exist on Mac. The maximize button is instead a kind
  // of toggle, but has no effect on frame decorations.
  EXPECT_FALSE(restore_button()->GetVisible());
  EXPECT_TRUE(maximize_button()->GetVisible());
#else
  EXPECT_TRUE(restore_button()->GetVisible());
  EXPECT_FALSE(maximize_button()->GetVisible());
#endif
}

// Tests that when the parent cannot maximize that the maximize button is not
// visible
TEST_F(CustomFrameViewTest, CannotMaximizeHidesButton) {
  widget()->widget_delegate()->SetCanMaximize(false);

  widget()->SetBounds(gfx::Rect(0, 0, 300, 100));
  widget()->Show();

  EXPECT_FALSE(restore_button()->GetVisible());
  EXPECT_FALSE(maximize_button()->GetVisible());
}

// Tests that when the parent cannot minimize that the minimize button is not
// visible
TEST_F(CustomFrameViewTest, CannotMinimizeHidesButton) {
  widget()->widget_delegate()->SetCanMinimize(false);

  widget()->SetBounds(gfx::Rect(0, 0, 300, 100));
  widget()->Show();

  EXPECT_FALSE(minimize_button()->GetVisible());
}

// Tests that when maximized that the edge button has an increased width.
TEST_F(CustomFrameViewTest, LargerEdgeButtonsWhenMaximized) {
  // Custom ordering to have a button on each edge.
  std::vector<views::FrameButton> leading;
  leading.push_back(views::FrameButton::kClose);
  leading.push_back(views::FrameButton::kMaximize);
  std::vector<views::FrameButton> trailing;
  trailing.push_back(views::FrameButton::kMinimize);
  SetWindowButtonOrder(leading, trailing);

  widget()->SetBounds(gfx::Rect(0, 0, 300, 100));
  widget()->Show();

  gfx::Rect close_button_initial_bounds = close_button()->bounds();
  gfx::Rect minimize_button_initial_bounds = minimize_button()->bounds();

  widget()->Maximize();
  // Just calling Maximize() doesn't invlidate the layout immediately.
  custom_frame_view()->InvalidateLayout();
  views::test::RunScheduledLayout(custom_frame_view());

#if BUILDFLAG(IS_MAC)
  // On Mac, "Maximize" should not alter the frame. Only fullscreen does that.
  EXPECT_EQ(close_button()->bounds().width(),
            close_button_initial_bounds.width());
  EXPECT_EQ(minimize_button()->bounds().width(),
            minimize_button_initial_bounds.width());
#else
  EXPECT_GT(close_button()->bounds().width(),
            close_button_initial_bounds.width());
  EXPECT_GT(minimize_button()->bounds().width(),
            minimize_button_initial_bounds.width());
#endif
}

}  // namespace views

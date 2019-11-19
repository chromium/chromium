// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/custom_frame_view.h"

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/window_button_order_provider.h"

namespace views {

namespace {

// Allows for the control of whether or not the widget can minimize/maximize or
// not. This can be set after initial setup in order to allow testing of both
// forms of delegates. By default this can minimize and maximize.
class MinimizeAndMaximizeStateControlDelegate : public WidgetDelegateView {
 public:
  MinimizeAndMaximizeStateControlDelegate() = default;
  ~MinimizeAndMaximizeStateControlDelegate() override = default;

  void set_can_maximize(bool can_maximize) {
    can_maximize_ = can_maximize;
  }

  void set_can_minimize(bool can_minimize) {
    can_minimize_ = can_minimize;
  }

  // WidgetDelegate:
  bool CanMaximize() const override { return can_maximize_; }
  bool CanMinimize() const override { return can_minimize_; }

 private:
  bool can_maximize_ = true;
  bool can_minimize_ = true;

  DISALLOW_COPY_AND_ASSIGN(MinimizeAndMaximizeStateControlDelegate);
};

}  // namespace

class CustomFrameViewTest : public ViewsTestBase {
 public:
  CustomFrameViewTest() = default;
  ~CustomFrameViewTest() override = default;

  CustomFrameView* custom_frame_view() {
    return custom_frame_view_;
  }

  MinimizeAndMaximizeStateControlDelegate*
        minimize_and_maximize_state_control_delegate() {
    return minimize_and_maximize_state_control_delegate_;
  }

  Widget* widget() {
    return widget_;
  }

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
    return custom_frame_view_->minimize_button_;
  }

  ImageButton* maximize_button() {
    return custom_frame_view_->maximize_button_;
  }

  ImageButton* restore_button() {
    return custom_frame_view_->restore_button_;
  }

  ImageButton* close_button() {
    return custom_frame_view_->close_button_;
  }

  gfx::Rect title_bounds() {
    return custom_frame_view_->title_bounds_;
  }

  void SetWindowButtonOrder(
      const std::vector<views::FrameButton> leading_buttons,
      const std::vector<views::FrameButton> trailing_buttons);

 private:
  // Parent container for |custom_frame_view_|
  Widget* widget_;

  // Owned by |widget_|
  CustomFrameView* custom_frame_view_;

  // Delegate of |widget_| which controls minimizing and maximizing
  MinimizeAndMaximizeStateControlDelegate*
        minimize_and_maximize_state_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewTest);
};

void CustomFrameViewTest::SetUp() {
  ViewsTestBase::SetUp();

  minimize_and_maximize_state_control_delegate_ =
      new MinimizeAndMaximizeStateControlDelegate;
  widget_ = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.delegate = minimize_and_maximize_state_control_delegate_;
  params.remove_standard_frame = true;
  widget_->Init(std::move(params));

  custom_frame_view_ = new CustomFrameView;
  widget_->non_client_view()->SetFrameView(custom_frame_view_);
}

void CustomFrameViewTest::TearDown() {
  widget_->CloseNow();

  ViewsTestBase::TearDown();
}

void CustomFrameViewTest::SetWindowButtonOrder(
    const std::vector<views::FrameButton> leading_buttons,
    const std::vector<views::FrameButton> trailing_buttons) {
  WindowButtonOrderProvider::GetInstance()->
      SetWindowButtonOrder(leading_buttons, trailing_buttons);
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
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();
  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();

  EXPECT_LT(minimize_button()->x(), maximize_button()->x());
  EXPECT_LT(maximize_button()->x(), close_button()->x());
  EXPECT_FALSE(restore_button()->GetVisible());

  EXPECT_GT(minimize_button()->x(),
            title_bounds().x() + title_bounds().width());
}

// Tests that setting the buttons to leading places them before the title.
TEST_F(CustomFrameViewTest, LeadingButtonLayout) {
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();

  std::vector<views::FrameButton> leading;
  leading.push_back(views::FrameButton::kClose);
  leading.push_back(views::FrameButton::kMinimize);
  leading.push_back(views::FrameButton::kMaximize);

  std::vector<views::FrameButton> trailing;

  SetWindowButtonOrder(leading, trailing);

  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();
  EXPECT_LT(close_button()->x(), minimize_button()->x());
  EXPECT_LT(minimize_button()->x(), maximize_button()->x());
  EXPECT_FALSE(restore_button()->GetVisible());
  EXPECT_LT(maximize_button()->x() + maximize_button()->width(),
            title_bounds().x());
}

// Tests that layouts occurring while maximized swap the maximize button for the
// restore button
TEST_F(CustomFrameViewTest, MaximizeRevealsRestoreButton) {
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();
  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();

  ASSERT_FALSE(restore_button()->GetVisible());
  ASSERT_TRUE(maximize_button()->GetVisible());

  parent->Maximize();
  view->Layout();

#if defined(OS_MACOSX)
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
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();
  MinimizeAndMaximizeStateControlDelegate* delegate =
        minimize_and_maximize_state_control_delegate();
  delegate->set_can_maximize(false);

  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();

  EXPECT_FALSE(restore_button()->GetVisible());
  EXPECT_FALSE(maximize_button()->GetVisible());
}

// Tests that when the parent cannot minimize that the minimize button is not
// visible
TEST_F(CustomFrameViewTest, CannotMinimizeHidesButton) {
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();
  MinimizeAndMaximizeStateControlDelegate* delegate =
      minimize_and_maximize_state_control_delegate();
  delegate->set_can_minimize(false);

  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();

  EXPECT_FALSE(minimize_button()->GetVisible());
}

// Tests that when maximized that the edge button has an increased width.
TEST_F(CustomFrameViewTest, LargerEdgeButtonsWhenMaximized) {
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();

  // Custom ordering to have a button on each edge.
  std::vector<views::FrameButton> leading;
  leading.push_back(views::FrameButton::kClose);
  leading.push_back(views::FrameButton::kMaximize);
  std::vector<views::FrameButton> trailing;
  trailing.push_back(views::FrameButton::kMinimize);
  SetWindowButtonOrder(leading, trailing);

  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();

  gfx::Rect close_button_initial_bounds = close_button()->bounds();
  gfx::Rect minimize_button_initial_bounds = minimize_button()->bounds();

  parent->Maximize();
  view->Layout();

#if defined(OS_MACOSX)
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

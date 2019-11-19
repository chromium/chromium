// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/resize_area.h"

#include <memory>

#include "base/bind.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

#if !defined(OS_MACOSX)
#include "ui/aura/window.h"
#endif

namespace {
// Constants used by the ResizeAreaTest.SuccessfulGestureDrag test to simulate
// a gesture drag by |kGestureScrollDistance| resulting from
// |kGestureScrollSteps| ui::ET_GESTURE_SCROLL_UPDATE events being delivered.
const int kGestureScrollDistance = 100;
const int kGestureScrollSteps = 4;
const int kDistancePerGestureScrollUpdate =
    kGestureScrollDistance / kGestureScrollSteps;
}

namespace views {

// Testing delegate used by ResizeAreaTest.
class TestResizeAreaDelegate : public ResizeAreaDelegate {
 public:
  TestResizeAreaDelegate();
  ~TestResizeAreaDelegate() override;

  // ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

  int resize_amount() { return resize_amount_; }
  bool done_resizing() { return done_resizing_; }
  bool on_resize_called() { return on_resize_called_; }

 private:
  int resize_amount_ = 0;
  bool done_resizing_ = false;
  bool on_resize_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestResizeAreaDelegate);
};

TestResizeAreaDelegate::TestResizeAreaDelegate() = default;

TestResizeAreaDelegate::~TestResizeAreaDelegate() = default;

void TestResizeAreaDelegate::OnResize(int resize_amount, bool done_resizing) {
  resize_amount_ = resize_amount;
  done_resizing_ = done_resizing;
  on_resize_called_ = true;
}

// Test fixture for testing the ResizeArea class.
class ResizeAreaTest : public ViewsTestBase {
 public:
  ResizeAreaTest();
  ~ResizeAreaTest() override;

  // Callback used by the SuccessfulGestureDrag test.
  void ProcessGesture(ui::EventType type, const gfx::Vector2dF& delta);

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

  int resize_amount() { return delegate_->resize_amount(); }
  bool done_resizing() { return delegate_->done_resizing(); }
  bool on_resize_called() { return delegate_->on_resize_called(); }
  views::Widget* widget() { return widget_; }

 private:
  std::unique_ptr<TestResizeAreaDelegate> delegate_;
  ResizeArea* resize_area_ = nullptr;
  views::Widget* widget_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  // The number of ui::ET_GESTURE_SCROLL_UPDATE events seen by
  // ProcessGesture().
  int gesture_scroll_updates_seen_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ResizeAreaTest);
};

ResizeAreaTest::ResizeAreaTest() = default;

ResizeAreaTest::~ResizeAreaTest() = default;

void ResizeAreaTest::ProcessGesture(ui::EventType type,
                                    const gfx::Vector2dF& delta) {
  if (type == ui::ET_GESTURE_SCROLL_BEGIN) {
    EXPECT_FALSE(done_resizing());
    EXPECT_FALSE(on_resize_called());
  } else if (type == ui::ET_GESTURE_SCROLL_UPDATE) {
    gesture_scroll_updates_seen_++;
    EXPECT_EQ(kDistancePerGestureScrollUpdate * gesture_scroll_updates_seen_,
              resize_amount());
    EXPECT_FALSE(done_resizing());
    EXPECT_TRUE(on_resize_called());
  } else if (type == ui::ET_GESTURE_SCROLL_END) {
    EXPECT_TRUE(done_resizing());
  }
}

void ResizeAreaTest::SetUp() {
  views::ViewsTestBase::SetUp();

  delegate_ = std::make_unique<TestResizeAreaDelegate>();
  resize_area_ = new ResizeArea(delegate_.get());

  gfx::Size size(10, 10);
  resize_area_->SetBounds(0, 0, size.width(), size.height());

  views::Widget::InitParams init_params(
      CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  init_params.bounds = gfx::Rect(size);

  widget_ = new views::Widget();
  widget_->Init(std::move(init_params));
  widget_->SetContentsView(resize_area_);
  widget_->Show();

  event_generator_ =
      std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_));
}

void ResizeAreaTest::TearDown() {
  if (widget_ && !widget_->IsClosed())
    widget_->Close();

  views::ViewsTestBase::TearDown();
}

// TODO(tdanderson): Enable these tests on OSX. See crbug.com/710475.
#if !defined(OS_MACOSX)
// Verifies the correct calls have been made to
// TestResizeAreaDelegate::OnResize() for a sequence of mouse events
// corresponding to a successful resize operation.
TEST_F(ResizeAreaTest, SuccessfulMouseDrag) {
  event_generator()->MoveMouseToCenterOf(widget()->GetNativeView());
  event_generator()->PressLeftButton();

  const int kFirstDragAmount = -5;
  event_generator()->MoveMouseBy(kFirstDragAmount, 0);
  EXPECT_EQ(kFirstDragAmount, resize_amount());
  EXPECT_FALSE(done_resizing());
  EXPECT_TRUE(on_resize_called());

  const int kSecondDragAmount = 17;
  event_generator()->MoveMouseBy(kSecondDragAmount, 0);
  EXPECT_EQ(kFirstDragAmount + kSecondDragAmount, resize_amount());
  EXPECT_FALSE(done_resizing());

  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(kFirstDragAmount + kSecondDragAmount, resize_amount());
  EXPECT_TRUE(done_resizing());
}

// Verifies that no resize is performed when attempting to resize using the
// right mouse button.
TEST_F(ResizeAreaTest, FailedMouseDrag) {
  event_generator()->MoveMouseToCenterOf(widget()->GetNativeView());
  event_generator()->PressRightButton();

  const int kDragAmount = 18;
  event_generator()->MoveMouseBy(kDragAmount, 0);
  EXPECT_EQ(0, resize_amount());
}

// Verifies the correct calls have been made to
// TestResizeAreaDelegate::OnResize() for a sequence of gesture events
// corresponding to a successful resize operation.
TEST_F(ResizeAreaTest, SuccessfulGestureDrag) {
  gfx::Point start = widget()->GetNativeView()->bounds().CenterPoint();
  event_generator()->GestureScrollSequenceWithCallback(
      start, gfx::Point(start.x() + kGestureScrollDistance, start.y()),
      base::TimeDelta::FromMilliseconds(200), kGestureScrollSteps,
      base::BindRepeating(&ResizeAreaTest::ProcessGesture,
                          base::Unretained(this)));
}

// Verifies that no resize is performed on a gesture tap.
TEST_F(ResizeAreaTest, NoDragOnGestureTap) {
  event_generator()->GestureTapAt(
      widget()->GetNativeView()->bounds().CenterPoint());

  EXPECT_EQ(0, resize_amount());
}
#endif  // !defined(OS_MACOSX)

}  // namespace views

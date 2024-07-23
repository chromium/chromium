// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/resize_area.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

#if !BUILDFLAG(IS_MAC)
#include "ui/aura/window.h"
#endif

namespace {
// Constants used by the ResizeAreaTest.SuccessfulGestureDrag test to simulate
// a gesture drag by |kGestureScrollDistance| resulting from
// |kGestureScrollSteps| ui::EventType::kGestureScrollUpdate events being
// delivered.
const int kGestureScrollDistance = 100;
const int kGestureScrollSteps = 4;
const int kDistancePerGestureScrollUpdate =
    kGestureScrollDistance / kGestureScrollSteps;
}  // namespace

namespace views {

// Testing delegate used by ResizeAreaTest.
class TestResizeAreaDelegate : public ResizeAreaDelegate {
 public:
  TestResizeAreaDelegate();

  TestResizeAreaDelegate(const TestResizeAreaDelegate&) = delete;
  TestResizeAreaDelegate& operator=(const TestResizeAreaDelegate&) = delete;

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

  ResizeAreaTest(const ResizeAreaTest&) = delete;
  ResizeAreaTest& operator=(const ResizeAreaTest&) = delete;

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
  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<TestResizeAreaDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  // The number of ui::EventType::kGestureScrollUpdate events seen by
  // ProcessGesture().
  int gesture_scroll_updates_seen_ = 0;
};

ResizeAreaTest::ResizeAreaTest() = default;

ResizeAreaTest::~ResizeAreaTest() = default;

void ResizeAreaTest::ProcessGesture(ui::EventType type,
                                    const gfx::Vector2dF& delta) {
  if (type == ui::EventType::kGestureScrollBegin) {
    EXPECT_FALSE(done_resizing());
    EXPECT_FALSE(on_resize_called());
  } else if (type == ui::EventType::kGestureScrollUpdate) {
    gesture_scroll_updates_seen_++;
    EXPECT_EQ(kDistancePerGestureScrollUpdate * gesture_scroll_updates_seen_,
              resize_amount());
    EXPECT_FALSE(done_resizing());
    EXPECT_TRUE(on_resize_called());
  } else if (type == ui::EventType::kGestureScrollEnd) {
    EXPECT_TRUE(done_resizing());
  }
}

void ResizeAreaTest::SetUp() {
  views::ViewsTestBase::SetUp();

  delegate_ = std::make_unique<TestResizeAreaDelegate>();
  auto resize_area = std::make_unique<ResizeArea>(delegate_.get());

  gfx::Size size(10, 10);
  resize_area->SetBounds(0, 0, size.width(), size.height());

  views::Widget::InitParams init_params(
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
  init_params.bounds = gfx::Rect(size);

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(init_params));
  widget_->SetContentsView(std::move(resize_area));
  widget_->Show();

  event_generator_ =
      std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_.get()));
}

void ResizeAreaTest::TearDown() {
  if (widget_ && !widget_->IsClosed()) {
    widget_.reset();
  }

  views::ViewsTestBase::TearDown();
}

// TODO(tdanderson): Enable these tests on OSX. See crbug.com/710475.
#if !BUILDFLAG(IS_MAC)
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
      base::Milliseconds(200), kGestureScrollSteps,
      base::BindRepeating(&ResizeAreaTest::ProcessGesture,
                          base::Unretained(this)));
}

// Verifies that no resize is performed on a gesture tap.
TEST_F(ResizeAreaTest, NoDragOnGestureTap) {
  event_generator()->GestureTapAt(
      widget()->GetNativeView()->bounds().CenterPoint());

  EXPECT_EQ(0, resize_amount());
}

TEST_F(ResizeAreaTest, AccessibleRole) {
  auto* resize_area = widget()->GetContentsView();
  ui::AXNodeData data;
  resize_area->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kSplitter);
  EXPECT_EQ(resize_area->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kSplitter);

  data = ui::AXNodeData();
  resize_area->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  resize_area->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(resize_area->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kButton);
}

#endif  // !BUILDFLAG(IS_MAC)

}  // namespace views

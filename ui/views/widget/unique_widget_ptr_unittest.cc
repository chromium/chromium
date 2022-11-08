// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/unique_widget_ptr.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class UniqueWidgetPtrTest : public ViewsTestBase, public WidgetObserver {
 public:
  UniqueWidgetPtrTest() = default;
  ~UniqueWidgetPtrTest() override = default;

  // ViewsTestBase overrides.
  void TearDown() override {
    ViewsTestBase::TearDown();
    ASSERT_EQ(widget_, nullptr);
  }

 protected:
  std::unique_ptr<Widget> AllocateTestWidget() override {
    auto widget = ViewsTestBase::AllocateTestWidget();
    widget->Init(CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS));
    widget_observation_.Observe(widget.get());
    return widget;
  }

  UniqueWidgetPtr CreateUniqueWidgetPtr() {
    auto widget = UniqueWidgetPtr(AllocateTestWidget());
    widget->SetContentsView(std::make_unique<View>());
    widget_ = widget.get();
    return widget;
  }

  Widget* widget() { return widget_; }

  // WidgetObserver overrides.
  void OnWidgetDestroying(Widget* widget) override {
    ASSERT_NE(widget_, nullptr);
    ASSERT_EQ(widget_, widget);
    ASSERT_TRUE(widget_observation_.IsObservingSource(widget_.get()));
    widget_observation_.Reset();
    widget_ = nullptr;
  }

 private:
  raw_ptr<Widget> widget_ = nullptr;
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

// Make sure explicitly resetting the |unique_widget_ptr| variable properly
// closes the widget. TearDown() will ensure |widget_| has been cleared.
TEST_F(UniqueWidgetPtrTest, TestCloseContent) {
  UniqueWidgetPtr unique_widget_ptr = CreateUniqueWidgetPtr();
  EXPECT_EQ(unique_widget_ptr->GetContentsView(), widget()->GetContentsView());
  unique_widget_ptr.reset();
}

// Same as above, only testing that going out of scope will accomplish the same
// thing.
TEST_F(UniqueWidgetPtrTest, TestScopeDestruct) {
  UniqueWidgetPtr unique_widget_ptr = CreateUniqueWidgetPtr();
  EXPECT_EQ(unique_widget_ptr->GetContentsView(), widget()->GetContentsView());
  // Just go out of scope to close the view;
}

// Check that proper move semantics for assignments work.
TEST_F(UniqueWidgetPtrTest, TestMoveAssign) {
  UniqueWidgetPtr unique_widget_ptr2 = CreateUniqueWidgetPtr();
  {
    UniqueWidgetPtr unique_widget_ptr;
    EXPECT_EQ(unique_widget_ptr2->GetContentsView(),
              widget()->GetContentsView());
    unique_widget_ptr = std::move(unique_widget_ptr2);
    EXPECT_EQ(unique_widget_ptr->GetContentsView(),
              widget()->GetContentsView());
    EXPECT_FALSE(unique_widget_ptr2);  // NOLINT
    unique_widget_ptr.reset();
    EXPECT_FALSE(unique_widget_ptr);
  }
  RunPendingMessages();
  EXPECT_EQ(widget(), nullptr);
}

// Check that move construction functions correctly.
TEST_F(UniqueWidgetPtrTest, TestMoveConstruct) {
  UniqueWidgetPtr unique_widget_ptr2 = CreateUniqueWidgetPtr();
  {
    EXPECT_EQ(unique_widget_ptr2->GetContentsView(),
              widget()->GetContentsView());
    UniqueWidgetPtr unique_widget_ptr = std::move(unique_widget_ptr2);
    EXPECT_EQ(unique_widget_ptr->GetContentsView(),
              widget()->GetContentsView());
    EXPECT_FALSE(unique_widget_ptr2);  // NOLINT
    unique_widget_ptr.reset();
    EXPECT_FALSE(unique_widget_ptr);
  }
  RunPendingMessages();
  EXPECT_EQ(widget(), nullptr);
}

// Make sure that any external closing of the widget is properly tracked in the
// |unique_widget_ptr|.
TEST_F(UniqueWidgetPtrTest, TestCloseWidget) {
  UniqueWidgetPtr unique_widget_ptr = CreateUniqueWidgetPtr();
  EXPECT_EQ(unique_widget_ptr->GetContentsView(), widget()->GetContentsView());
  // Initiate widget destruction.
  widget()->CloseWithReason(Widget::ClosedReason::kUnspecified);
  // Cycle the run loop to allow the deferred destruction to happen.
  RunPendingMessages();
  // The UniqueWidgetPtr should have dropped its reference to the content view.
  EXPECT_FALSE(unique_widget_ptr);
}

// When the NativeWidget is destroyed, ensure that the Widget is also destroyed
// which in turn clears the |unique_widget_ptr|.
TEST_F(UniqueWidgetPtrTest, TestCloseNativeWidget) {
  UniqueWidgetPtr unique_widget_ptr = CreateUniqueWidgetPtr();
  EXPECT_EQ(unique_widget_ptr->GetContentsView(), widget()->GetContentsView());
  // Initiate an OS level native widget destruction.
  SimulateNativeDestroy(widget());
  // The UniqueWidgetPtr should have dropped its reference to the content view.
  EXPECT_FALSE(unique_widget_ptr);
}

}  // namespace views

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
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace views {

class UniqueWidgetPtrTest
    : public ViewsTestBase,
      public ::testing::WithParamInterface<Widget::InitParams::Ownership>,
      public ViewObserver {
 public:
  UniqueWidgetPtrTest() = default;
  ~UniqueWidgetPtrTest() override = default;

  // ViewsTestBase overrides.
  void TearDown() override {
    ViewsTestBase::TearDown();
    ASSERT_EQ(widget_, nullptr);
    ASSERT_EQ(root_view_, nullptr);
  }

 protected:
  std::unique_ptr<Widget> AllocateTestWidget() override {
    auto widget = ViewsTestBase::AllocateTestWidget();
    widget->Init(
        CreateParams(GetParam(), Widget::InitParams::TYPE_WINDOW_FRAMELESS));
    root_view_observation_.Observe(widget->GetRootView());
    return widget;
  }

  UniqueWidgetPtr CreateUniqueWidgetPtr() {
    auto widget = UniqueWidgetPtr(AllocateTestWidget());
    widget->SetContentsView(std::make_unique<View>());
    widget_ = widget.get();
    root_view_ = widget->GetRootView();
    return widget;
  }

  Widget* widget() { return widget_; }

  // WidgetObserver overrides.
  void OnViewIsDeleting(View* observed_view) override {
    // Observing the deletion of the root view is more reliable than observing
    // `WidgetObserver::OnWidgetDestroying/Destroyed()`. The latter can still
    // be called when the native widget is destroyed, but the actual `Widget`
    // itself is still alive (and both should be getting destroyed).
    ASSERT_NE(root_view_, nullptr);
    ASSERT_EQ(observed_view, root_view_);
    ASSERT_TRUE(root_view_observation_.IsObservingSource(root_view_));
    root_view_observation_.Reset();
    widget_ = nullptr;
    root_view_ = nullptr;
  }

 protected:
  raw_ptr<Widget> widget_ = nullptr;
  raw_ptr<View> root_view_ = nullptr;
  base::ScopedObservation<View, ViewObserver> root_view_observation_{this};
};

// Make sure explicitly resetting the |unique_widget_ptr| variable properly
// closes the widget. TearDown() will ensure |widget_| has been cleared.
TEST_P(UniqueWidgetPtrTest, TestCloseContent) {
  UniqueWidgetPtr unique_widget_ptr = CreateUniqueWidgetPtr();
  EXPECT_EQ(unique_widget_ptr->GetContentsView(), widget()->GetContentsView());
  unique_widget_ptr.reset();
}

// Same as above, only testing that going out of scope will accomplish the same
// thing.
TEST_P(UniqueWidgetPtrTest, TestScopeDestruct) {
  UniqueWidgetPtr unique_widget_ptr = CreateUniqueWidgetPtr();
  EXPECT_EQ(unique_widget_ptr->GetContentsView(), widget()->GetContentsView());
  // Just go out of scope to close the view;
}

// Check that proper move semantics for assignments work.
TEST_P(UniqueWidgetPtrTest, TestMoveAssign) {
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
TEST_P(UniqueWidgetPtrTest, TestMoveConstruct) {
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
TEST_P(UniqueWidgetPtrTest, TestCloseWidget) {
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
TEST_P(UniqueWidgetPtrTest, TestCloseNativeWidget) {
  UniqueWidgetPtr unique_widget_ptr = CreateUniqueWidgetPtr();
  EXPECT_EQ(unique_widget_ptr->GetContentsView(), widget()->GetContentsView());
  // Initiate an OS level native widget destruction.
  SimulateNativeDestroy(widget());
  // The UniqueWidgetPtr should have dropped its reference to the content view.
  EXPECT_FALSE(unique_widget_ptr);
}

INSTANTIATE_TEST_SUITE_P(
    AllOwnershipTypes,
    UniqueWidgetPtrTest,
    testing::Values(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                    Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                    Widget::InitParams::CLIENT_OWNS_WIDGET));

}  // namespace views

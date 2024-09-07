// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/widget_focus_observer.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Dummy supplier that doesn't do anything.
class DummyWidgetFocusSupplier : public test::internal::WidgetFocusSupplier {
 public:
  DummyWidgetFocusSupplier() = default;
  ~DummyWidgetFocusSupplier() override = default;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

 protected:
  Widget::Widgets GetAllWidgets() const override {
    return test::WidgetTest::GetAllWidgets();
  }
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(DummyWidgetFocusSupplier)

}  // namespace

class WidgetFocusObserverTest : public ViewsTestBase {
 public:
  WidgetFocusObserverTest() {
    frame_.supplier_list().MaybeRegister<DummyWidgetFocusSupplier>();
  }
  ~WidgetFocusObserverTest() override = default;

 private:
  test::internal::WidgetFocusSupplierFrame frame_;
};

TEST_F(WidgetFocusObserverTest, NoWidgets) {
  test::WidgetFocusObserver observer;
  observer.SetStateObserverStateChangedCallback(base::DoNothing());
  EXPECT_EQ(gfx::NativeView(), observer.GetStateObserverInitialState());
}

TEST_F(WidgetFocusObserverTest, OneWidget) {
  const auto widget = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  test::WidgetVisibleWaiter visible_waiter(widget.get());
  widget->Show();
  visible_waiter.Wait();

  test::WidgetFocusObserver observer;
  observer.SetStateObserverStateChangedCallback(base::DoNothing());
  EXPECT_EQ(widget->GetNativeView(), observer.GetStateObserverInitialState());
}

TEST_F(WidgetFocusObserverTest, SeveralWidgets) {
  const auto widget1 = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  test::WidgetVisibleWaiter visible_waiter1(widget1.get());
  widget1->Show();
  visible_waiter1.Wait();

  const auto widget2 = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  test::WidgetVisibleWaiter visible_waiter2(widget1.get());
  widget2->Show();
  visible_waiter2.Wait();

  const auto widget3 = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  test::WidgetVisibleWaiter visible_waiter3(widget1.get());
  widget3->ShowInactive();
  visible_waiter3.Wait();

  test::WidgetFocusObserver observer;
  observer.SetStateObserverStateChangedCallback(base::DoNothing());
  EXPECT_EQ(widget2->GetNativeView(), observer.GetStateObserverInitialState());
}

TEST_F(WidgetFocusObserverTest, AfterActivate) {
  const auto widget1 = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  test::WidgetVisibleWaiter visible_waiter1(widget1.get());
  widget1->Show();
  visible_waiter1.Wait();

  const auto widget2 = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  test::WidgetVisibleWaiter visible_waiter2(widget1.get());
  widget2->Show();
  visible_waiter2.Wait();

  const auto widget3 = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  test::WidgetVisibleWaiter visible_waiter3(widget1.get());
  widget3->ShowInactive();
  visible_waiter3.Wait();

  widget3->Activate();
  test::WaitForWidgetActive(widget3.get(), true);

  test::WidgetFocusObserver observer;
  observer.SetStateObserverStateChangedCallback(base::DoNothing());
  EXPECT_EQ(widget3->GetNativeView(), observer.GetStateObserverInitialState());
}

TEST_F(WidgetFocusObserverTest, Bubble) {
  const auto widget = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  test::WidgetVisibleWaiter visible_waiter(widget.get());
  widget->Show();
  visible_waiter.Wait();

  auto bubble = std::make_unique<BubbleDialogDelegateView>(
      widget->GetRootView(), BubbleBorder::LEFT_CENTER);
  auto* const bubble_widget =
      BubbleDialogDelegate::CreateBubble(std::move(bubble));
  test::WidgetVisibleWaiter visible_waiter2(bubble_widget);
  bubble_widget->Show();
  visible_waiter2.Wait();

  test::WidgetFocusObserver observer;
  observer.SetStateObserverStateChangedCallback(base::DoNothing());
  EXPECT_EQ(bubble_widget->GetNativeView(),
            observer.GetStateObserverInitialState());
}

}  // namespace views

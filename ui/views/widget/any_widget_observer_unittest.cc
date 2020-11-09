// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/any_widget_observer.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/widget_test.h"

namespace {

using views::Widget;

using AnyWidgetObserverTest = views::test::WidgetTest;

TEST_F(AnyWidgetObserverTest, ObservesInitialize) {
  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});

  bool initialized = false;

  observer.set_initialized_callback(
      base::BindLambdaForTesting([&](Widget*) { initialized = true; }));

  EXPECT_FALSE(initialized);
  WidgetAutoclosePtr w0(WidgetTest::CreateTopLevelPlatformWidget());
  EXPECT_TRUE(initialized);
}

TEST_F(AnyWidgetObserverTest, ObservesClose) {
  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});

  bool closing = false;

  observer.set_closing_callback(
      base::BindLambdaForTesting([&](Widget*) { closing = true; }));

  EXPECT_FALSE(closing);
  { WidgetAutoclosePtr w0(WidgetTest::CreateTopLevelPlatformWidget()); }
  EXPECT_TRUE(closing);
}

TEST_F(AnyWidgetObserverTest, ObservesShow) {
  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});

  bool shown = false;

  observer.set_shown_callback(
      base::BindLambdaForTesting([&](Widget*) { shown = true; }));

  EXPECT_FALSE(shown);
  WidgetAutoclosePtr w0(WidgetTest::CreateTopLevelPlatformWidget());
  w0->Show();
  EXPECT_TRUE(shown);
}

TEST_F(AnyWidgetObserverTest, ObservesHide) {
  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});

  bool hidden = false;

  observer.set_hidden_callback(
      base::BindLambdaForTesting([&](Widget*) { hidden = true; }));

  EXPECT_FALSE(hidden);
  WidgetAutoclosePtr w0(WidgetTest::CreateTopLevelPlatformWidget());
  w0->Hide();
  EXPECT_TRUE(hidden);
}

class NamedWidgetShownWaiterTest : public views::test::WidgetTest {
 public:
  NamedWidgetShownWaiterTest() = default;
  ~NamedWidgetShownWaiterTest() override = default;

  views::Widget* CreateNamedWidget(const std::string& name) {
    Widget* widget = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.native_widget = views::test::CreatePlatformNativeWidgetImpl(
        widget, views::test::kStubCapture, nullptr);
    params.name = name;
    widget->Init(std::move(params));
    return widget;
  }
};

TEST_F(NamedWidgetShownWaiterTest, ShownAfterWait) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "TestWidget");

  WidgetAutoclosePtr w0(CreateNamedWidget("TestWidget"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce([](views::Widget* widget) { widget->Show(); },
                                base::Unretained(w0.get())));
  EXPECT_EQ(waiter.WaitIfNeededAndGet(), w0.get());
}

TEST_F(NamedWidgetShownWaiterTest, ShownBeforeWait) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "TestWidget");
  WidgetAutoclosePtr w0(CreateNamedWidget("TestWidget"));
  w0->Show();
  EXPECT_EQ(waiter.WaitIfNeededAndGet(), w0.get());
}

TEST_F(NamedWidgetShownWaiterTest, OtherWidgetShown) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "TestWidget");
  WidgetAutoclosePtr w0(CreateNamedWidget("NotTestWidget"));
  WidgetAutoclosePtr w1(CreateNamedWidget("TestWidget"));
  w0->Show();
  w1->Show();
  EXPECT_EQ(waiter.WaitIfNeededAndGet(), w1.get());
}

}  // namespace

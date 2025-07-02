// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace views {

class AnyWidgetObserverInteractiveTest : public test::WidgetTest {
 public:
  AnyWidgetObserverInteractiveTest() = default;
  AnyWidgetObserverInteractiveTest(const AnyWidgetObserverInteractiveTest&) =
      delete;
  AnyWidgetObserverInteractiveTest& operator=(
      const AnyWidgetObserverInteractiveTest&) = delete;
  ~AnyWidgetObserverInteractiveTest() override = default;

  void SetUp() override {
    SetUpForInteractiveTests();
    WidgetTest::SetUp();
  }
};

TEST_F(AnyWidgetObserverInteractiveTest, ObservesActivate) {
  AnyWidgetObserver observer(test::AnyWidgetTestPasskey{});

  Widget* activated_widget = nullptr;
  observer.set_activated_callback(base::BindLambdaForTesting(
      [&](Widget* widget) { activated_widget = widget; }));

  std::unique_ptr<Widget> w0 = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  w0->Show();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return activated_widget == w0.get(); }));

  std::unique_ptr<Widget> w1 = base::WrapUnique(
      CreateTopLevelPlatformWidget(Widget::InitParams::CLIENT_OWNS_WIDGET));
  w1->Show();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return activated_widget == w1.get(); }));
  EXPECT_FALSE(w0->IsActive());

  w0->Activate();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return activated_widget == w0.get(); }));
  EXPECT_FALSE(w1->IsActive());

  w1->Activate();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return activated_widget == w1.get(); }));
  EXPECT_FALSE(w0->IsActive());

  // Destroy the current active widget. The previous active widget should be
  // activated again.
  w1.reset();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return activated_widget == w0.get(); }));
}

}  // namespace views

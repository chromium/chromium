// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_activation_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "ui/views/test/mock_activation_controller.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

namespace {

class WidgetActivationDelegateTest : public ViewsTestBase,
                                     public WidgetObserver {
 public:
  WidgetActivationDelegateTest() = default;
  WidgetActivationDelegateTest(const WidgetActivationDelegateTest&) = delete;
  WidgetActivationDelegateTest& operator=(const WidgetActivationDelegateTest&) =
      delete;
  ~WidgetActivationDelegateTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    ViewsTestBase::SetUp();
  }

  // WidgetObserver:
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    visibility_[widget] = visible;
  }

  void OnWidgetVisibilityOnScreenChanged(Widget* widget,
                                         bool visible) override {
    visibility_on_screen_[widget] = visible;
  }

  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    activation_[widget] = active;
  }

  std::optional<bool> GetVisibility(Widget* widget) {
    CHECK(visibility_.count(widget));
    return visibility_[widget];
  }
  std::optional<bool> GetVisibilityOnScreen(Widget* widget) {
    if (visibility_on_screen_.count(widget)) {
      return visibility_on_screen_[widget];
    }
    return std::nullopt;
  }
  std::optional<bool> GetActivation(Widget* widget) {
    if (activation_.count(widget)) {
      return activation_[widget];
    }
    return std::nullopt;
  }

  std::unique_ptr<Widget> CreateWidget(std::string name) {
    auto widget = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    params.name = name;
    widget->Init(std::move(params));
    widget->AddObserver(this);
    return widget;
  }

  void ResetAll() {
    visibility_.clear();
    visibility_on_screen_.clear();
    activation_.clear();
  }

 private:
  // Do not use raw_ptr, as the widget may be deleted by the time we need
  // to check.
  base::flat_map<Widget*, bool> visibility_;
  base::flat_map<Widget*, bool> visibility_on_screen_;
  base::flat_map<Widget*, bool> activation_;

  test::MockActivationController mock_activation_controller_;
};

}  // namespace

TEST_F(WidgetActivationDelegateTest, ActivationStateChanges) {
  auto widget1 = CreateWidget("Widget1");
  auto widget2 = CreateWidget("Widget2");
  auto widget3 = CreateWidget("Widget3");

  // Show widget1. It should be active.
  widget1->Show();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_TRUE(GetVisibility(widget1.get()).value());
  EXPECT_TRUE(GetActivation(widget1.get()).value());
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(GetVisibilityOnScreen(widget1.get()).value());
#endif
  ResetAll();

  // Show widget2. widget1 should be deactivated, widget2 should be activated.
  widget2->Show();
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget2->IsActive());

  EXPECT_FALSE(GetActivation(widget1.get()).value());
  EXPECT_TRUE(GetVisibility(widget2.get()).value());
  EXPECT_TRUE(GetActivation(widget2.get()).value());
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(GetVisibilityOnScreen(widget2.get()).value());
#endif
  ResetAll();

  // Show widget3. widget2 should be deactivated, widget3 should be activated.
  widget3->Show();
  EXPECT_FALSE(widget2->IsActive());
  EXPECT_TRUE(widget3->IsActive());
  EXPECT_FALSE(GetActivation(widget2.get()).value());
  EXPECT_TRUE(GetVisibility(widget3.get()).value());
  EXPECT_TRUE(GetActivation(widget3.get()).value());
  ResetAll();

  // Activate widget1. widget3 should be deactivated, widget1 should be
  // activated.
  widget1->Activate();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget3->IsActive());
  EXPECT_TRUE(GetActivation(widget1.get()).value());
  EXPECT_FALSE(GetActivation(widget3.get()).value());
  ResetAll();

#if !BUILDFLAG(IS_MAC)
  // Deactivate widget1. It should fall back to widget3 (last active).
  widget1->Deactivate();
#else
  // MacOS does not support Deactivate. Just activate widget3.
  widget3->Activate();
#endif
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget3->IsActive());

  EXPECT_FALSE(GetActivation(widget1.get()).value());
  EXPECT_TRUE(GetActivation(widget3.get()).value());
  ResetAll();

  // Hide widget3. It should fall back to widget1.
  widget3->Hide();
  EXPECT_FALSE(widget3->IsActive());
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(GetVisibility(widget3.get()).value());
  EXPECT_FALSE(GetActivation(widget3.get()).value());
  EXPECT_TRUE(GetActivation(widget1.get()).value());
#if BUILDFLAG(IS_MAC)
  EXPECT_FALSE(GetVisibilityOnScreen(widget3.get()).value());
#endif
  ResetAll();

  // Close widget1. It should fall back to widget2.
  widget1->CloseNow();
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_FALSE(widget3->IsActive());
  EXPECT_TRUE(GetActivation(widget2.get()).value());
  ResetAll();

  // Show widget 3 again
  widget3->Show();
  EXPECT_TRUE(widget3->IsActive());
  EXPECT_FALSE(widget2->IsActive());
  EXPECT_FALSE(GetActivation(widget2.get()).value());
  EXPECT_TRUE(GetActivation(widget3.get()).value());
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(GetVisibilityOnScreen(widget3.get()).value());
#endif
  ResetAll();

  // Close widget3. It should fall back to widget2.
  widget3->CloseNow();
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_TRUE(GetActivation(widget2.get()).value());
}

TEST_F(WidgetActivationDelegateTest, ShowInactive) {
  auto widget1 = CreateWidget("Widget1");
  auto widget2 = CreateWidget("Widget2");
  auto widget3 = CreateWidget("Widget3");

  widget1->Show();
  widget2->Show();
  widget3->ShowInactive();

  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_FALSE(widget3->IsActive());
  EXPECT_FALSE(GetActivation(widget1.get()).value());
  EXPECT_TRUE(GetActivation(widget2.get()).value());
  EXPECT_TRUE(GetVisibility(widget1.get()).value());
  EXPECT_TRUE(GetVisibility(widget2.get()).value());
  EXPECT_TRUE(GetVisibility(widget3.get()).value());

  ResetAll();

  // Closing inactive widget should not affect activation.
  widget3->CloseNow();
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_FALSE(GetActivation(widget1.get()));
  EXPECT_FALSE(GetActivation(widget2.get()));
}

TEST_F(WidgetActivationDelegateTest, NonActivatable) {
  auto widget1 = CreateWidget("Widget1");
  auto widget2 = CreateWidget("Widget2");
  auto widget3 = CreateWidget("Widget3");

  widget1->Show();
  widget2->Show();
  widget3->widget_delegate()->SetCanActivate(false);
  ASSERT_FALSE(widget3->CanActivate());
  widget3->Show();

  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_FALSE(widget3->IsActive());
  EXPECT_FALSE(GetActivation(widget1.get()).value());
  EXPECT_TRUE(GetActivation(widget2.get()).value());
  EXPECT_TRUE(GetVisibility(widget1.get()).value());
  EXPECT_TRUE(GetVisibility(widget2.get()).value());
  EXPECT_TRUE(GetVisibility(widget3.get()).value());

  ResetAll();

  // Activate is no-op on inactivatable widget.
  widget3->Activate();
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_FALSE(widget3->IsActive());

  // Closing 2 should not activate 3.
  widget2->CloseNow();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget3->IsActive());
}

}  // namespace views

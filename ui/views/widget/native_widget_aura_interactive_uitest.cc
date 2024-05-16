// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/focus_controller.h"

namespace views::test {

namespace {

class TestFocusRules : public wm::BaseFocusRules {
 public:
  TestFocusRules() = default;

  TestFocusRules(const TestFocusRules&) = delete;
  TestFocusRules& operator=(const TestFocusRules&) = delete;

  ~TestFocusRules() override = default;

  void set_can_activate(bool can_activate) { can_activate_ = can_activate; }

  // wm::BaseFocusRules overrides:
  bool SupportsChildActivation(const aura::Window* window) const override {
    return true;
  }

  bool CanActivateWindow(const aura::Window* window) const override {
    return can_activate_;
  }

 private:
  bool can_activate_ = true;
};

}  // namespace

using NativeWidgetAuraTest = DesktopWidgetTestInteractive;

// When requesting view focus from a non-active top level widget, focus is not
// instantly given. Instead, the view is firstly stored and then it is attempted
// to activate the widget. If widget is currently not activatable, focus should
// not be grabbed. And focus will be given/restored the next time the widget is
// made active. (crbug.com/621791)
TEST_F(NativeWidgetAuraTest, NonActiveWindowRequestImeFocus) {
  TestFocusRules* test_focus_rules = new TestFocusRules;
  std::unique_ptr<wm::FocusController> focus_controller =
      std::make_unique<wm::FocusController>(test_focus_rules);
  wm::SetActivationClient(GetContext(), focus_controller.get());

  auto widget1 = std::make_unique<Widget>();
  Widget::InitParams params1(Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET,
                             Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params1.context = GetContext();
  params1.native_widget =
      CreatePlatformNativeWidgetImpl(widget1.get(), kDefault, nullptr);
  widget1->Init(std::move(params1));
  auto* textfield1 =
      widget1->GetRootView()->AddChildView(std::make_unique<Textfield>());

  auto widget2 = std::make_unique<Widget>();
  Widget::InitParams params2(Widget::InitParams::CLIENT_OWNS_WIDGET,
                             Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params2.context = GetContext();
  params2.native_widget =
      CreatePlatformNativeWidgetImpl(widget2.get(), kDefault, nullptr);
  widget2->Init(std::move(params2));
  auto* textfield2a =
      widget2->GetRootView()->AddChildView(std::make_unique<Textfield>());
  auto* textfield2b =
      widget2->GetRootView()->AddChildView(std::make_unique<Textfield>());

  widget1->Show();
  views::test::WaitForWidgetActive(widget1.get(), true);
  textfield1->RequestFocus();
  EXPECT_TRUE(textfield1->HasFocus());
  EXPECT_FALSE(textfield2a->HasFocus());
  EXPECT_FALSE(textfield2b->HasFocus());

  // Don't allow window activation at this step.
  test_focus_rules->set_can_activate(false);
  textfield2a->RequestFocus();
  EXPECT_TRUE(textfield1->HasFocus());
  EXPECT_FALSE(textfield2a->HasFocus());
  EXPECT_FALSE(textfield2b->HasFocus());

  // Allow window activation and |widget2| gets activated at this step, focus
  // should be properly restored.
  test_focus_rules->set_can_activate(true);
  widget2->Activate();
  views::test::WaitForWidgetActive(widget2.get(), true);
  EXPECT_TRUE(textfield2a->HasFocus());
  EXPECT_FALSE(textfield2b->HasFocus());
  EXPECT_FALSE(textfield1->HasFocus());

  widget1->CloseNow();
  widget2->CloseNow();
}

}  // namespace views::test

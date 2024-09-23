// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/wm/public/activation_client.h"

namespace views::test {

using DesktopNativeWidgetAuraTest = DesktopWidgetTestInteractive;

// This tests ensures that when a widget with an active child widget are
// showing, and a new widget is shown, the widget and its child are both
// deactivated. This covers a regression where deactivating the child widget
// would activate the parent widget at the same time the new widget receives
// activation, causing windows to lock when minimizing / maximizing (see
// crbug.com/1284537).
TEST_F(DesktopNativeWidgetAuraTest, WidgetsWithChildrenDeactivateCorrectly) {
  auto widget1 = std::make_unique<Widget>();
  Widget::InitParams params1(Widget::InitParams::CLIENT_OWNS_WIDGET,
                             Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params1.context = GetContext();
  params1.native_widget = new DesktopNativeWidgetAura(widget1.get());
  widget1->Init(std::move(params1));

  auto widget1_child = std::make_unique<Widget>();
  Widget::InitParams params_child(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_BUBBLE);
  params_child.parent = widget1->GetNativeView();
  params_child.native_widget =
      CreatePlatformNativeWidgetImpl(widget1_child.get(), kDefault, nullptr);
  widget1_child->Init(std::move(params_child));
  widget1_child->widget_delegate()->SetCanActivate(true);

  auto widget2 = std::make_unique<Widget>();
  Widget::InitParams params2(Widget::InitParams::CLIENT_OWNS_WIDGET,
                             Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params2.context = GetContext();
  params2.native_widget = new DesktopNativeWidgetAura(widget2.get());
  widget2->Init(std::move(params2));

  auto* activation_client1 =
      wm::GetActivationClient(widget1->GetNativeView()->GetRootWindow());
  auto* activation_client1_child =
      wm::GetActivationClient(widget1_child->GetNativeView()->GetRootWindow());
  auto* activation_client2 =
      wm::GetActivationClient(widget2->GetNativeView()->GetRootWindow());

  // All widgets belonging to the same tree host should share an activation
  // client. Widgets belonging to different tree hosts should have different
  // activation clients.
  ASSERT_EQ(activation_client1, activation_client1_child);
  ASSERT_NE(activation_client1, activation_client2);

  const auto show_widget = [&](Widget* target) {
    target->Show();
    views::test::WaitForWidgetActive(widget1.get(), target == widget1.get());
    views::test::WaitForWidgetActive(widget1_child.get(),
                                     target == widget1_child.get());
    views::test::WaitForWidgetActive(widget2.get(), target == widget2.get());
  };

  show_widget(widget1.get());
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget1_child->IsActive());
  EXPECT_FALSE(widget2->IsActive());
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1->GetNativeView());
  EXPECT_EQ(activation_client2->GetActiveWindow(), nullptr);

  // The child widget should become activate and its parent should deactivate.
  show_widget(widget1_child.get());
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget1_child->IsActive());
  EXPECT_FALSE(widget2->IsActive());
  EXPECT_EQ(activation_client1->GetActiveWindow(),
            widget1_child->GetNativeView());
  EXPECT_EQ(activation_client2->GetActiveWindow(), nullptr);

  // Showing the second widget should deactivate both the first widget and its
  // child.
  show_widget(widget2.get());
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_FALSE(widget1_child->IsActive());
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_EQ(activation_client1->GetActiveWindow(), nullptr);
  EXPECT_EQ(activation_client2->GetActiveWindow(), widget2->GetNativeView());

  widget1_child->CloseNow();
  widget1->CloseNow();
  widget2->CloseNow();
}

// Tests to make sure that a widget that shows an active child has activation
// correctly propagate to the child's content window. This also tests to make
// sure that when this child window is closed, and the desktop widget's window
// tree host remains active, the widget's content window has its activation
// state restored. This tests against a regression where the desktop widget
// would not receive activation when it's child bubbles were closed (see
// crbug.com/1294404).
TEST_F(DesktopNativeWidgetAuraTest,
       DesktopWidgetsRegainFocusWhenChildWidgetClosed) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                            Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.context = GetContext();
  params.native_widget = new DesktopNativeWidgetAura(widget.get());
  widget->Init(std::move(params));

  auto widget_child = std::make_unique<Widget>();
  Widget::InitParams params_child(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_BUBBLE);
  params_child.parent = widget->GetNativeView();
  params_child.native_widget =
      CreatePlatformNativeWidgetImpl(widget_child.get(), kDefault, nullptr);
  widget_child->Init(std::move(params_child));
  widget_child->widget_delegate()->SetCanActivate(true);

  auto* activation_client =
      wm::GetActivationClient(widget->GetNativeView()->GetRootWindow());
  auto* activation_client_child =
      wm::GetActivationClient(widget_child->GetNativeView()->GetRootWindow());

  // All widgets belonging to the same tree host should share an activation
  // client.
  ASSERT_EQ(activation_client, activation_client_child);

  widget->Show();
  views::test::WaitForWidgetActive(widget.get(), true);
  views::test::WaitForWidgetActive(widget_child.get(), false);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(widget_child->IsActive());
  EXPECT_EQ(activation_client->GetActiveWindow(), widget->GetNativeView());

  widget_child->Show();
  views::test::WaitForWidgetActive(widget.get(), false);
  views::test::WaitForWidgetActive(widget_child.get(), true);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(widget_child->IsActive());
  EXPECT_EQ(activation_client->GetActiveWindow(),
            widget_child->GetNativeView());

  widget_child->Close();
  views::test::WaitForWidgetActive(widget.get(), true);
  views::test::WaitForWidgetActive(widget_child.get(), false);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(widget_child->IsActive());
  EXPECT_EQ(activation_client->GetActiveWindow(), widget->GetNativeView());

  widget_child->CloseNow();
  widget->CloseNow();
}

}  // namespace views::test

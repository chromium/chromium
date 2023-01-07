// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_focus_rules.h"

#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace views {

using DesktopFocusRulesTest = test::DesktopWidgetTest;

// Verifies we don't attempt to activate a window in another widget.
TEST_F(DesktopFocusRulesTest, DontFocusWindowsInOtherHierarchies) {
  // Two widgets (each with a DesktopNativeWidgetAura). |w2| has a child Window
  // |w2_child| that is not focusable. |w2_child|'s has a transient parent in
  // |w1|.
  Widget* w1 = CreateTopLevelNativeWidget();
  Widget* w2 = CreateTopLevelNativeWidget();
  aura::test::TestWindowDelegate w2_child_delegate;
  w2_child_delegate.set_can_focus(false);
  aura::Window* w2_child = new aura::Window(&w2_child_delegate);
  w2_child->Init(ui::LAYER_SOLID_COLOR);
  w2->GetNativeView()->AddChild(w2_child);
  wm::AddTransientChild(w1->GetNativeView(), w2_child);
  aura::client::GetFocusClient(w2->GetNativeView())->FocusWindow(w2_child);
  aura::Window* focused =
      aura::client::GetFocusClient(w2->GetNativeView())->GetFocusedWindow();
  EXPECT_TRUE((focused == nullptr) || w2->GetNativeView()->Contains(focused));
  wm::RemoveTransientChild(w1->GetNativeView(), w2_child);

  w1->CloseNow();
  w2->CloseNow();
}

// Verifies root windows are not activatable.
TEST_F(DesktopFocusRulesTest, CanActivateWindowForRootWindow) {
  Widget* w1 = CreateTopLevelNativeWidget();
  aura::Window* content_window = w1->GetNativeWindow();
  aura::Window* root_window = content_window->GetRootWindow();
  EXPECT_TRUE(wm::CanActivateWindow(content_window));
  EXPECT_FALSE(wm::CanActivateWindow(root_window));
  w1->CloseNow();
}

}  // namespace views

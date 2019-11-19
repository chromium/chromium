// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_menu_runner_views.h"

#include "base/macros.h"
#include "ui/events/event_utils.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace {

// Should match |kMenuButtonWidth| in touch_selection_menu_runner_views.cc.
const int kMenuButtonWidth = 63;

// Should match size of |kMenuCommands| array in
// touch_selection_menu_runner_views.cc.
const int kMenuCommandCount = 3;

}

class TouchSelectionMenuRunnerViewsTest : public ViewsTestBase,
                                          public ui::TouchSelectionMenuClient {
 public:
  TouchSelectionMenuRunnerViewsTest() = default;
  ~TouchSelectionMenuRunnerViewsTest() override = default;

 protected:
  void SetUp() override {
    ViewsTestBase::SetUp();
    // These tests expect NativeWidgetAura and so aren't applicable to
    // aura-mus-client. http://crbug.com/663561.
  }

  void set_no_commmand_available(bool no_command) {
    no_command_available_ = no_command;
  }

  int last_executed_command_id() const { return last_executed_command_id_; }

 private:
  // ui::TouchSelectionMenuClient:
  bool IsCommandIdEnabled(int command_id) const override {
    return !no_command_available_;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    last_executed_command_id_ = command_id;
  }

  void RunContextMenu() override {}

  base::string16 GetSelectedText() override { return base::string16(); }

  bool ShouldShowQuickMenu() override { return false; }

  // When set to true, no command would be available and menu should not be
  // shown.
  bool no_command_available_ = false;

  int last_executed_command_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionMenuRunnerViewsTest);
};

// Tests that the default touch selection menu runner is installed and opening
// and closing the menu works properly.
TEST_F(TouchSelectionMenuRunnerViewsTest, InstalledAndWorksProperly) {
  gfx::Rect menu_anchor(0, 0, 10, 10);
  gfx::Size handle_size(10, 10);

  // Menu runner instance should be installed, but no menu should be running.
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Run menu. Since commands are available, this should bring up menus.
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, menu_anchor, handle_size, GetContext());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Close menu.
  ui::TouchSelectionMenuRunner::GetInstance()->CloseMenu();
  RunPendingMessages();
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Try running menu when no commands is available. Menu should not be shown.
  set_no_commmand_available(true);
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, menu_anchor, handle_size, GetContext());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests that anchor rect for the quick menu is adjusted correctly based on the
// distance of handles.
TEST_F(TouchSelectionMenuRunnerViewsTest, QuickMenuAdjustsAnchorRect) {
  gfx::Size handle_size(10, 10);
  TouchSelectionMenuRunnerViews::TestApi test_api(
      static_cast<TouchSelectionMenuRunnerViews*>(
          ui::TouchSelectionMenuRunner::GetInstance()));

  // Calculate the width of quick menu. In addition to |kMenuCommandCount|
  // commands, there is an item for ellipsis.
  int quick_menu_width =
      (kMenuCommandCount + 1) * kMenuButtonWidth + kMenuCommandCount;

  // Set anchor rect's width a bit smaller than the quick menu width plus handle
  // image width and check that anchor rect's height is adjusted.
  gfx::Rect anchor_rect(0, 0, quick_menu_width + handle_size.width() - 10, 20);
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, anchor_rect, handle_size, GetContext());
  anchor_rect.Inset(0, 0, 0, -handle_size.height());
  EXPECT_EQ(anchor_rect, test_api.GetAnchorRect());

  // Set anchor rect's width a bit greater than the quick menu width plus handle
  // image width and check that anchor rect's height is not adjusted.
  anchor_rect =
      gfx::Rect(0, 0, quick_menu_width + handle_size.width() + 10, 20);
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, anchor_rect, handle_size, GetContext());
  EXPECT_EQ(anchor_rect, test_api.GetAnchorRect());

  ui::TouchSelectionMenuRunner::GetInstance()->CloseMenu();
  RunPendingMessages();
}

// Tests that running one of menu actions closes the menu properly.
TEST_F(TouchSelectionMenuRunnerViewsTest, RunningActionClosesProperly) {
  gfx::Rect menu_anchor(0, 0, 10, 10);
  gfx::Size handle_size(10, 10);
  TouchSelectionMenuRunnerViews::TestApi test_api(
      static_cast<TouchSelectionMenuRunnerViews*>(
          ui::TouchSelectionMenuRunner::GetInstance()));

  // Initially, no menu should be running.
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Run menu. Since commands are available, this should bring up menus.
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, menu_anchor, handle_size, GetContext());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Tap the first action on the menu and check taht the menu is closed
  // properly.
  LabelButton* button = test_api.GetFirstButton();
  DCHECK(button);
  gfx::Point button_center = button->bounds().CenterPoint();
  ui::GestureEventDetails details(ui::ET_GESTURE_TAP);
  details.set_tap_count(1);
  ui::GestureEvent tap(button_center.x(), button_center.y(), 0,
                       ui::EventTimeForNow(), details);
  button->OnGestureEvent(&tap);
  RunPendingMessages();
  EXPECT_NE(0, last_executed_command_id());
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

// Tests that closing the menu widget cleans up the menu runner state properly.
TEST_F(TouchSelectionMenuRunnerViewsTest, ClosingWidgetClosesProperly) {
  gfx::Rect menu_anchor(0, 0, 10, 10);
  gfx::Size handle_size(10, 10);
  TouchSelectionMenuRunnerViews::TestApi test_api(
      static_cast<TouchSelectionMenuRunnerViews*>(
          ui::TouchSelectionMenuRunner::GetInstance()));

  // Initially, no menu should be running.
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Run menu. Since commands are available, this should bring up menus.
  ui::TouchSelectionMenuRunner::GetInstance()->OpenMenu(
      this, menu_anchor, handle_size, GetContext());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());

  // Close the menu widget and check that menu runner correctly knows that menu
  // is not running anymore.
  Widget* widget = test_api.GetWidget();
  DCHECK(widget);
  widget->Close();
  RunPendingMessages();
  EXPECT_FALSE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
}

}  // namespace views

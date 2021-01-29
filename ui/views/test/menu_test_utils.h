// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_MENU_TEST_UTILS_H_
#define UI_VIEWS_TEST_MENU_TEST_UTILS_H_

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/test/test_views_delegate.h"

namespace views {

class MenuController;

namespace test {

// Test implementation of MenuDelegate that tracks calls to MenuDelegate, along
// with the provided parameters.
class TestMenuDelegate : public MenuDelegate {
 public:
  TestMenuDelegate();
  ~TestMenuDelegate() override;

  int show_context_menu_count() { return show_context_menu_count_; }
  MenuItemView* show_context_menu_source() { return show_context_menu_source_; }
  int execute_command_id() const { return execute_command_id_; }
  int on_menu_closed_called() const { return on_menu_closed_called_count_; }
  MenuItemView* on_menu_closed_menu() const { return on_menu_closed_menu_; }
  bool on_perform_drop_called() { return on_perform_drop_called_; }
  int will_hide_menu_count() { return will_hide_menu_count_; }
  MenuItemView* will_hide_menu() { return will_hide_menu_; }

  // MenuDelegate:
  bool ShowContextMenu(MenuItemView* source,
                       int id,
                       const gfx::Point& p,
                       ui::MenuSourceType source_type) override;
  void ExecuteCommand(int id) override;
  void OnMenuClosed(MenuItemView* menu) override;
  int OnPerformDrop(MenuItemView* menu,
                    DropPosition position,
                    const ui::DropTargetEvent& event) override;
  int GetDragOperations(MenuItemView* sender) override;
  void WriteDragData(MenuItemView* sender, OSExchangeData* data) override;
  void WillHideMenu(MenuItemView* menu) override;

 private:
  // The number of times ShowContextMenu was called.
  int show_context_menu_count_ = 0;

  // The value of the last call to ShowContextMenu.
  MenuItemView* show_context_menu_source_ = nullptr;

  // ID of last executed command.
  int execute_command_id_ = 0;

  // The number of times OnMenuClosed was called.
  int on_menu_closed_called_count_ = 0;

  // The value of the last call to OnMenuClosed.
  MenuItemView* on_menu_closed_menu_ = nullptr;

  // The number of times WillHideMenu was called.
  int will_hide_menu_count_ = 0;

  // The value of the last call to WillHideMenu.
  MenuItemView* will_hide_menu_ = nullptr;

  bool on_perform_drop_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestMenuDelegate);
};

// Test api which caches the currently active MenuController. Can be used to
// toggle visibility, and to clear seletion states, without performing full
// shutdown. This is used to simulate menus with varing states, such as during
// drags, without performing the entire operation. Used to test strange shutdown
// ordering.
class MenuControllerTestApi {
 public:
  MenuControllerTestApi();
  ~MenuControllerTestApi();

  MenuController* controller() { return controller_.get(); }

  // Clears out the current and pending states, without notifying the associated
  // menu items.
  void ClearState();

  // Toggles the internal showing state of |controller_| without attempting
  // to change associated Widgets.
  void SetShowing(bool showing);

 private:
  base::WeakPtr<MenuController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MenuControllerTestApi);
};

// On platforms which have menu closure animations, these functions are
// necessary to:
//   1) Disable those animations (make them take zero time) to avoid slowing
//      down tests;
//   2) Wait for maybe-asynchronous menu closure to finish.
// On platforms without menu closure animations, these do nothing.
void DisableMenuClosureAnimations();
void WaitForMenuClosureAnimation();

// An implementation of TestViewsDelegate which overrides ReleaseRef in order to
// call a provided callback.
class ReleaseRefTestViewsDelegate : public TestViewsDelegate {
 public:
  ReleaseRefTestViewsDelegate();
  ReleaseRefTestViewsDelegate(const ReleaseRefTestViewsDelegate&) = delete;
  ReleaseRefTestViewsDelegate& operator=(const ReleaseRefTestViewsDelegate&) =
      delete;
  ~ReleaseRefTestViewsDelegate() override;

  void set_release_ref_callback(base::RepeatingClosure release_ref_callback) {
    release_ref_callback_ = std::move(release_ref_callback);
  }

  // TestViewsDelegate:
  void ReleaseRef() override;

 private:
  base::RepeatingClosure release_ref_callback_;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_MENU_TEST_UTILS_H_

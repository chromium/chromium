// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_MENU_TEST_UTILS_H_
#define UI_VIEWS_TEST_MENU_TEST_UTILS_H_

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/view.h"

namespace views {

class MenuController;

namespace test {

// Test implementation of MenuDelegate that tracks calls to MenuDelegate, along
// with the provided parameters.
class TestMenuDelegate : public MenuDelegate {
 public:
  TestMenuDelegate();

  TestMenuDelegate(const TestMenuDelegate&) = delete;
  TestMenuDelegate& operator=(const TestMenuDelegate&) = delete;

  ~TestMenuDelegate() override;

  int show_context_menu_count() const { return show_context_menu_count_; }
  MenuItemView* show_context_menu_source() { return show_context_menu_source_; }
  int execute_command_id() const { return execute_command_id_; }
  int on_menu_closed_called() const { return on_menu_closed_called_count_; }
  MenuItemView* on_menu_closed_menu() const { return on_menu_closed_menu_; }
  bool is_drop_performed() const { return is_drop_performed_; }
  int will_hide_menu_count() const { return will_hide_menu_count_; }
  MenuItemView* will_hide_menu() { return will_hide_menu_; }
  void set_should_execute_command_without_closing_menu(bool val) {
    should_execute_command_without_closing_menu_ = val;
  }

  // MenuDelegate:
  bool ShowContextMenu(MenuItemView* source,
                       int id,
                       const gfx::Point& p,
                       ui::MenuSourceType source_type) override;
  void ExecuteCommand(int id) override;
  void OnMenuClosed(MenuItemView* menu) override;
  views::View::DropCallback GetDropCallback(
      MenuItemView* menu,
      DropPosition position,
      const ui::DropTargetEvent& event) override;
  int GetDragOperations(MenuItemView* sender) override;
  void WriteDragData(MenuItemView* sender, OSExchangeData* data) override;
  void WillHideMenu(MenuItemView* menu) override;
  bool ShouldExecuteCommandWithoutClosingMenu(int id,
                                              const ui::Event& e) override;

 private:
  // Performs the drop operation and updates |output_drag_op| accordingly.
  void PerformDrop(const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // The number of times ShowContextMenu was called.
  int show_context_menu_count_ = 0;

  // The value of the last call to ShowContextMenu.
  raw_ptr<MenuItemView, DanglingUntriaged> show_context_menu_source_ = nullptr;

  // ID of last executed command.
  int execute_command_id_ = 0;

  // The number of times OnMenuClosed was called.
  int on_menu_closed_called_count_ = 0;

  // The value of the last call to OnMenuClosed.
  raw_ptr<MenuItemView, DanglingUntriaged> on_menu_closed_menu_ = nullptr;

  // The number of times WillHideMenu was called.
  int will_hide_menu_count_ = 0;

  // The value of the last call to WillHideMenu.
  raw_ptr<MenuItemView, DanglingUntriaged> will_hide_menu_ = nullptr;

  bool is_drop_performed_ = false;

  bool should_execute_command_without_closing_menu_ = false;
};

// Test api which caches the currently active MenuController. Can be used to
// toggle visibility, and to clear seletion states, without performing full
// shutdown. This is used to simulate menus with varing states, such as during
// drags, without performing the entire operation. Used to test strange shutdown
// ordering.
class MenuControllerTestApi {
 public:
  MenuControllerTestApi();

  MenuControllerTestApi(const MenuControllerTestApi&) = delete;
  MenuControllerTestApi& operator=(const MenuControllerTestApi&) = delete;

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

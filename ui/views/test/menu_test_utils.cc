// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/menu_test_utils.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "ui/views/controls/menu/menu_controller.h"

#if defined(OS_MACOSX)
#include "ui/views/controls/menu/menu_closure_animation_mac.h"
#endif

namespace views {
namespace test {

// TestMenuDelegate -----------------------------------------------------------

TestMenuDelegate::TestMenuDelegate() = default;

TestMenuDelegate::~TestMenuDelegate() = default;

bool TestMenuDelegate::ShowContextMenu(MenuItemView* source,
                                       int id,
                                       const gfx::Point& p,
                                       ui::MenuSourceType source_type) {
  show_context_menu_count_++;
  show_context_menu_source_ = source;
  return true;
}

void TestMenuDelegate::ExecuteCommand(int id) {
  execute_command_id_ = id;
}

void TestMenuDelegate::OnMenuClosed(MenuItemView* menu) {
  on_menu_closed_called_count_++;
  on_menu_closed_menu_ = menu;
}

int TestMenuDelegate::OnPerformDrop(MenuItemView* menu,
                                    DropPosition position,
                                    const ui::DropTargetEvent& event) {
  on_perform_drop_called_ = true;
  return ui::DragDropTypes::DRAG_COPY;
}

int TestMenuDelegate::GetDragOperations(MenuItemView* sender) {
  return ui::DragDropTypes::DRAG_COPY;
}

void TestMenuDelegate::WriteDragData(MenuItemView* sender,
                                     OSExchangeData* data) {}

void TestMenuDelegate::WillHideMenu(MenuItemView* menu) {
  will_hide_menu_count_++;
  will_hide_menu_ = menu;
}

// MenuControllerTestApi ------------------------------------------------------

MenuControllerTestApi::MenuControllerTestApi()
    : controller_(MenuController::GetActiveInstance()->AsWeakPtr()) {}

MenuControllerTestApi::~MenuControllerTestApi() = default;

void MenuControllerTestApi::ClearState() {
  if (!controller_)
    return;
  controller_->ClearStateForTest();
}

void MenuControllerTestApi::SetShowing(bool showing) {
  if (!controller_)
    return;
  controller_->showing_ = showing;
}

void DisableMenuClosureAnimations() {
#if defined(OS_MACOSX)
  MenuClosureAnimationMac::DisableAnimationsForTesting();
#endif
}

void WaitForMenuClosureAnimation() {
#if defined(OS_MACOSX)
  // TODO(https://crbug.com/982815): Replace this with Quit+Run.
  base::RunLoop().RunUntilIdle();
#endif
}

}  // namespace test
}  // namespace views

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner_impl.h"

#include <memory>

#include "build/build_config.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner_impl_adapter.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/events/win/system_event_state_lookup.h"
#endif

#if defined(USE_X11)
#include "ui/events/x/events_x_utils.h"  // nogncheck
#endif

namespace views {
namespace internal {

#if !defined(OS_MACOSX)
MenuRunnerImplInterface* MenuRunnerImplInterface::Create(
    ui::MenuModel* menu_model,
    int32_t run_types,
    base::RepeatingClosure on_menu_closed_callback) {
  return new MenuRunnerImplAdapter(menu_model,
                                   std::move(on_menu_closed_callback));
}
#endif

MenuRunnerImpl::MenuRunnerImpl(MenuItemView* menu)
    : menu_(menu),
      running_(false),
      delete_after_run_(false),
      for_drop_(false),
      controller_(nullptr),
      owns_controller_(false) {}

bool MenuRunnerImpl::IsRunning() const {
  return running_;
}

void MenuRunnerImpl::Release() {
  if (running_) {
    if (delete_after_run_)
      return;  // We already canceled.

    // The menu is running a nested run loop, we can't delete it now
    // otherwise the stack would be in a really bad state (many frames would
    // have deleted objects on them). Instead cancel the menu, when it returns
    // Holder will delete itself.
    delete_after_run_ = true;

    // Swap in a different delegate. That way we know the original MenuDelegate
    // won't be notified later on (when it's likely already been deleted).
    if (!empty_delegate_.get())
      empty_delegate_ = std::make_unique<MenuDelegate>();
    menu_->set_delegate(empty_delegate_.get());

    // Verify that the MenuController is still active. It may have been
    // destroyed out of order.
    if (controller_) {
      // Release is invoked when MenuRunner is destroyed. Assume this is
      // happening because the object referencing the menu has been destroyed
      // and the menu button is no longer valid.
      controller_->Cancel(MenuController::ExitType::kDestroyed);
      return;
    }
  }

  delete this;
}

void MenuRunnerImpl::RunMenuAt(Widget* parent,
                               MenuButtonController* button_controller,
                               const gfx::Rect& bounds,
                               MenuAnchorPosition anchor,
                               int32_t run_types) {
  closing_event_time_ = base::TimeTicks();
  if (running_) {
    // Ignore requests to show the menu while it's already showing. MenuItemView
    // doesn't handle this very well (meaning it crashes).
    return;
  }

  MenuController* controller = MenuController::GetActiveInstance();
  if (controller) {
    if ((run_types & MenuRunner::IS_NESTED) != 0) {
      if (controller->for_drop()) {
        controller->Cancel(MenuController::ExitType::kAll);
        controller = nullptr;
      } else {
        // Only nest the delegate when not cancelling drag-and-drop. When
        // cancelling this will become the root delegate of the new
        // MenuController
        controller->AddNestedDelegate(this);
      }
    } else {
      // There's some other menu open and we're not nested. Cancel the menu.
      controller->Cancel(MenuController::ExitType::kAll);
      if ((run_types & MenuRunner::FOR_DROP) == 0) {
        // We can't open another menu, otherwise the message loop would become
        // twice nested. This isn't necessarily a problem, but generally isn't
        // expected.
        return;
      }
      // Drop menus don't block the message loop, so it's ok to create a new
      // MenuController.
      controller = nullptr;
    }
  }

  running_ = true;
  for_drop_ = (run_types & MenuRunner::FOR_DROP) != 0;
  bool has_mnemonics = (run_types & MenuRunner::HAS_MNEMONICS) != 0;
  owns_controller_ = false;
  if (!controller) {
    // No menus are showing, show one.
    controller = new MenuController(for_drop_, this);
    owns_controller_ = true;
  }
  DCHECK((run_types & MenuRunner::COMBOBOX) == 0 ||
         (run_types & MenuRunner::EDITABLE_COMBOBOX) == 0);
  using ComboboxType = MenuController::ComboboxType;
  if (run_types & MenuRunner::COMBOBOX)
    controller->set_combobox_type(ComboboxType::kReadonly);
  else if (run_types & MenuRunner::EDITABLE_COMBOBOX)
    controller->set_combobox_type(ComboboxType::kEditable);
  else
    controller->set_combobox_type(ComboboxType::kNone);
  controller->set_send_gesture_events_to_owner(
      (run_types & MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER) != 0);
  controller->set_use_touchable_layout(
      (run_types & MenuRunner::USE_TOUCHABLE_LAYOUT) != 0);
  controller_ = controller->AsWeakPtr();
  menu_->set_controller(controller_.get());
  menu_->PrepareForRun(owns_controller_, has_mnemonics,
                       !for_drop_ && ShouldShowMnemonics(run_types));

  controller->Run(parent, button_controller, menu_, bounds, anchor,
                  (run_types & MenuRunner::CONTEXT_MENU) != 0,
                  (run_types & MenuRunner::NESTED_DRAG) != 0);
}

void MenuRunnerImpl::Cancel() {
  if (running_)
    controller_->Cancel(MenuController::ExitType::kAll);
}

base::TimeTicks MenuRunnerImpl::GetClosingEventTime() const {
  return closing_event_time_;
}

void MenuRunnerImpl::OnMenuClosed(NotifyType type,
                                  MenuItemView* menu,
                                  int mouse_event_flags) {
  if (controller_)
    closing_event_time_ = controller_->closing_event_time();
  menu_->RemoveEmptyMenus();
  menu_->set_controller(nullptr);

  if (owns_controller_ && controller_) {
    // We created the controller and need to delete it.
    delete controller_.get();
    owns_controller_ = false;
  }
  controller_ = nullptr;
  // Make sure all the windows we created to show the menus have been
  // destroyed.
  menu_->DestroyAllMenuHosts();
  if (delete_after_run_) {
    delete this;
    return;
  }
  running_ = false;
  if (menu_->GetDelegate()) {
    // Executing the command may also delete this.
    base::WeakPtr<MenuRunnerImpl> ref(weak_factory_.GetWeakPtr());
    if (menu && !for_drop_) {
      // Do not execute the menu that was dragged/dropped.
      menu_->GetDelegate()->ExecuteCommand(menu->GetCommand(),
                                           mouse_event_flags);
    }
    // Only notify the delegate if it did not delete this.
    if (ref && type == NOTIFY_DELEGATE)
      menu_->GetDelegate()->OnMenuClosed(menu);
  }
}

void MenuRunnerImpl::SiblingMenuCreated(MenuItemView* menu) {
  if (menu != menu_ && sibling_menus_.count(menu) == 0)
    sibling_menus_.insert(menu);
}

MenuRunnerImpl::~MenuRunnerImpl() {
  delete menu_;
  for (auto* sibling_menu : sibling_menus_)
    delete sibling_menu;
}

bool MenuRunnerImpl::ShouldShowMnemonics(int32_t run_types) {
  bool show_mnemonics = run_types & MenuRunner::SHOULD_SHOW_MNEMONICS;
  // Show mnemonics if the button has focus or alt is pressed.
#if defined(OS_WIN)
  show_mnemonics |= ui::win::IsAltPressed();
#elif defined(USE_X11)
  show_mnemonics |= ui::IsAltPressed();
#elif defined(OS_MACOSX)
  show_mnemonics = false;
#endif
  return show_mnemonics;
}

}  // namespace internal
}  // namespace views

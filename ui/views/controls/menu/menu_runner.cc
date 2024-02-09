// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner_handler.h"
#include "ui/views/controls/menu/menu_runner_impl.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

namespace views {

MenuRunner::MenuRunner(ui::MenuModel* menu_model,
                       int32_t run_types,
                       base::RepeatingClosure on_menu_closed_callback)
    : run_types_(run_types),
      impl_(internal::MenuRunnerImplInterface::Create(
          menu_model,
          run_types,
          std::move(on_menu_closed_callback))) {}

MenuRunner::MenuRunner(std::unique_ptr<MenuItemView> menu, int32_t run_types)
    : run_types_(run_types),
      impl_(new internal::MenuRunnerImpl(std::move(menu))) {}

MenuRunner::~MenuRunner() {
  // Release causes the deletion of the object.
  impl_.ExtractAsDangling()->Release();
}

void MenuRunner::RunMenuAt(
    Widget* parent,
    MenuButtonController* button_controller,
    const gfx::Rect& bounds,
    MenuAnchorPosition anchor,
    ui::MenuSourceType source_type,
    gfx::NativeView native_view_for_gestures,
    std::optional<gfx::RoundedCornersF> corners,
    std::optional<std::string> show_menu_host_duration_histogram) {
  // Do not attempt to show the menu if the application is currently shutting
  // down. MenuDelegate::OnMenuClosed would not be called.
  if (ViewsDelegate::GetInstance() &&
      ViewsDelegate::GetInstance()->IsShuttingDown()) {
    return;
  }

  // If we are shown on mouse press, we will eat the subsequent mouse down and
  // the parent widget will not be able to reset its state (it might have mouse
  // capture from the mouse down). So we clear its state here.
  if (parent && parent->GetRootView()) {
    auto* root_view = parent->GetRootView();
    if (run_types_ & MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER) {
      // In this case, the menu owner instead of the menu should handle the
      // incoming gesture events. Therefore we do not need to reset the gesture
      // handler of `root_view`.
      root_view->SetMouseHandler(nullptr);
    } else {
      root_view->SetMouseAndGestureHandler(nullptr);
    }
  }

  if (runner_handler_.get()) {
    runner_handler_->RunMenuAt(parent, button_controller, bounds, anchor,
                               source_type, run_types_);
    return;
  }

  if ((run_types_ & CONTEXT_MENU) && !(run_types_ & FIXED_ANCHOR)) {
    switch (source_type) {
      case ui::MENU_SOURCE_NONE:
      case ui::MENU_SOURCE_KEYBOARD:
      case ui::MENU_SOURCE_MOUSE:
        anchor = MenuAnchorPosition::kTopLeft;
        break;
      case ui::MENU_SOURCE_TOUCH:
      case ui::MENU_SOURCE_TOUCH_EDIT_MENU:
        anchor = MenuAnchorPosition::kBottomCenter;
        break;
      default:
        break;
    }
  }

  impl_->RunMenuAt(parent, button_controller, bounds, anchor, run_types_,
                   native_view_for_gestures, corners,
                   std::move(show_menu_host_duration_histogram));
}

bool MenuRunner::IsRunning() const {
  return impl_->IsRunning();
}

void MenuRunner::Cancel() {
  impl_->Cancel();
}

base::TimeTicks MenuRunner::closing_event_time() const {
  return impl_->GetClosingEventTime();
}

void MenuRunner::SetRunnerHandler(
    std::unique_ptr<MenuRunnerHandler> runner_handler) {
  runner_handler_ = std::move(runner_handler);
}

}  // namespace views

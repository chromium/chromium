// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_pre_target_handler_aura.h"

#include <memory>

#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace views {

namespace {

aura::Window* GetOwnerRootWindow(views::Widget* owner) {
  return owner ? owner->GetNativeWindow()->GetRootWindow() : nullptr;
}

}  // namespace

MenuPreTargetHandlerAura::MenuPreTargetHandlerAura(MenuController* controller,
                                                   Widget* owner)
    : controller_(controller), root_(GetOwnerRootWindow(owner)) {
  if (root_) {
    wm::GetActivationClient(root_)->AddObserver(this);
    root_->AddObserver(this);
  } else {
    // This should only happen in cases like when context menus are shown for
    // Windows OS system tray items and there is no parent window.
  }
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);
}

MenuPreTargetHandlerAura::~MenuPreTargetHandlerAura() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
  Cleanup();
}

void MenuPreTargetHandlerAura::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!controller_->drag_in_progress())
    controller_->Cancel(MenuController::ExitType::kAll);
}

void MenuPreTargetHandlerAura::OnWindowDestroying(aura::Window* window) {
  Cleanup();
}

void MenuPreTargetHandlerAura::OnCancelMode(ui::CancelModeEvent* event) {
  controller_->Cancel(MenuController::ExitType::kAll);
}

void MenuPreTargetHandlerAura::OnKeyEvent(ui::KeyEvent* event) {
  // Fully exit the menu, if the key event is supposed to perform some task.
  if (ShouldCancelMenuForEvent(*event)) {
    controller_->Cancel(MenuController::ExitType::kAll);
    return;
  }
  controller_->OnWillDispatchKeyEvent(event);
}

bool MenuPreTargetHandlerAura::ShouldCancelMenuForEvent(
    const ui::KeyEvent& event) {
  const ui::KeyboardCode key_code = event.key_code();
  switch (key_code) {
    case ui::VKEY_ESCAPE:
      // Fully exit the menu when Shift-Esc is pressed because it is
      // supposed to open the Task Manager on Windows and Linux
      if (event.IsShiftDown()) {
        return true;
      }
      break;
    case ui::VKEY_J:
      // Fully exit the menu when Ctrl+J is pressed because it is supposed
      // to open the downloads page on Windows and Linux.
      if (event.IsControlDown()) {
        return true;
      }
      break;
    case ui::VKEY_H:
    case ui::VKEY_R:
    case ui::VKEY_N:
    case ui::VKEY_T:
    case ui::VKEY_P:
    case ui::VKEY_S:
      // Fully exit the menu when:
      // Ctrl+H is pressed because it is supposed to open the history page.
      // Ctrl+R is pressed because it is supposed to reload the current page.
      // Ctrl+N/Ctrl+Shift+N is pressed because it is supposed to open the
      // new window/new incognito window.
      // Ctrl+T is pressed because it is supposed to open the new tab.
      // Ctrl+P is pressed because it is supposed to print the current page.
      // Ctrl+S is pressed because it is supposed to save the current web page.
      if (event.IsControlDown()) {
        return true;
      }
      break;
    default:
      break;
  }
  return false;
}

void MenuPreTargetHandlerAura::Cleanup() {
  if (!root_)
    return;
  // The ActivationClient may have been destroyed by the time we get here.
  wm::ActivationClient* client = wm::GetActivationClient(root_);
  if (client)
    client->RemoveObserver(this);
  root_->RemoveObserver(this);
  root_ = nullptr;
}

// static
std::unique_ptr<MenuPreTargetHandler> MenuPreTargetHandler::Create(
    MenuController* controller,
    Widget* owner) {
  return std::make_unique<MenuPreTargetHandlerAura>(controller, owner);
}

}  // namespace views

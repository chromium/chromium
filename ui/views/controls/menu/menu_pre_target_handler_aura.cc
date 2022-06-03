// Copyright 2016 The Chromium Authors. All rights reserved.
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
  controller_->OnWillDispatchKeyEvent(event);
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

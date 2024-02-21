// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner_impl_mac.h"

#include "components/remote_cocoa/browser/window.h"
#include "ui/views/controls/menu/menu_runner_impl_adapter.h"
#include "ui/views/controls/menu/menu_runner_impl_cocoa.h"
#include "ui/views/controls/menu/menu_runner_impl_remote_cocoa.h"
#include "ui/views/widget/widget.h"

namespace views::internal {

// static
MenuRunnerImplInterface* MenuRunnerImplInterface::Create(
    ui::MenuModel* menu_model,
    int32_t run_types,
    base::RepeatingClosure on_menu_closed_callback) {
  if ((run_types & MenuRunner::CONTEXT_MENU) &&
      !(run_types & (MenuRunner::IS_NESTED))) {
    return new MenuRunnerImplMac(menu_model,
                                 std::move(on_menu_closed_callback));
  }
  return new MenuRunnerImplAdapter(menu_model,
                                   std::move(on_menu_closed_callback));
}

MenuRunnerImplMac::MenuRunnerImplMac(
    ui::MenuModel* menu_model,
    base::RepeatingClosure on_menu_closed_callback)
    : menu_model_(menu_model),
      on_menu_closed_callback_(std::move(on_menu_closed_callback)) {}

bool MenuRunnerImplMac::IsRunning() const {
  return implementation_ ? implementation_->IsRunning() : false;
}

void MenuRunnerImplMac::Release() {
  delete this;
}

void MenuRunnerImplMac::RunMenuAt(
    Widget* parent,
    MenuButtonController* button_controller,
    const gfx::Rect& bounds,
    MenuAnchorPosition anchor,
    int32_t run_types,
    gfx::NativeView native_view_for_gestures,
    std::optional<gfx::RoundedCornersF> corners,
    std::optional<std::string> show_menu_host_duration_histogram) {
  if (!implementation_) {
    if (remote_cocoa::IsWindowRemote(parent->GetNativeWindow())) {
      implementation_ =
          new MenuRunnerImplRemoteCocoa(menu_model_, on_menu_closed_callback_);
    } else {
      implementation_ =
          new MenuRunnerImplCocoa(menu_model_, on_menu_closed_callback_);
    }
  }
  implementation_->RunMenuAt(parent, button_controller, bounds, anchor,
                             run_types, native_view_for_gestures, corners,
                             show_menu_host_duration_histogram);
}

void MenuRunnerImplMac::Cancel() {
  if (implementation_) {
    implementation_->Cancel();
  }
}

base::TimeTicks MenuRunnerImplMac::GetClosingEventTime() const {
  return implementation_ ? implementation_->GetClosingEventTime()
                         : base::TimeTicks();
}

MenuRunnerImplMac::~MenuRunnerImplMac() {
  if (implementation_) {
    implementation_.ExtractAsDangling()->Release();
  }
}

}  // namespace views::internal

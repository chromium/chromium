// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner_impl_adapter.h"

#include <utility>

#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner_impl.h"

namespace views::internal {

MenuRunnerImplAdapter::MenuRunnerImplAdapter(
    ui::MenuModel* menu_model,
    base::RepeatingClosure on_menu_done_callback)
    : menu_model_adapter_(
          new MenuModelAdapter(menu_model, std::move(on_menu_done_callback))),
      impl_(new MenuRunnerImpl(menu_model_adapter_->CreateMenu())) {}

bool MenuRunnerImplAdapter::IsRunning() const {
  return impl_->IsRunning();
}

void MenuRunnerImplAdapter::Release() {
  // Stop observing the model right away, as if `menu_model_adapter_` was
  // destroyed, so that further model changes don't rebuild the released menu.
  menu_model_adapter_->DetachFromMenuModel();
  // `impl_` may defer its own destruction, e.g. while the MenuController's call
  // stack is active. `menu_model_adapter_` may still be in use during that
  // time, e.g. by MenuModelAdapter::BuildMenu() rebuilding the menu further up
  // the call stack. Hand ownership of it to `impl_` so that it lives as long as
  // the menu it built.
  impl_->TakeOwnershipOfDelegate(std::move(menu_model_adapter_));
  // Release will cause `impl_` to delete itself.
  impl_.ExtractAsDangling()->Release();
  delete this;
}

void MenuRunnerImplAdapter::RunMenuAt(
    Widget* parent,
    MenuButtonController* button_controller,
    const gfx::Rect& bounds,
    MenuAnchorPosition anchor,
    ui::mojom::MenuSourceType source_type,
    int32_t types,
    gfx::NativeView native_view_for_gestures,
    std::optional<gfx::RoundedCornersF> corners,
    std::optional<std::string> show_menu_host_duration_histogram) {
  impl_->RunMenuAt(parent, button_controller, bounds, anchor, source_type,
                   types, native_view_for_gestures);
}

void MenuRunnerImplAdapter::Cancel() {
  impl_->Cancel();
}

base::TimeTicks MenuRunnerImplAdapter::GetClosingEventTime() const {
  return impl_->GetClosingEventTime();
}

MenuRunnerImplAdapter::~MenuRunnerImplAdapter() = default;

}  // namespace views::internal

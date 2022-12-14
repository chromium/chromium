// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/views_text_services_context_menu_chromeos.h"

#include <utility>

#include "base/no_destructor.h"
#include "ui/views/controls/views_text_services_context_menu_base.h"

namespace views {

namespace {

using ImplFactory = ViewsTextServicesContextMenuChromeos::ImplFactory;
ImplFactory& GetImplFactory() {
  static base::NoDestructor<ImplFactory> impl_factory;
  return *impl_factory;
}

}  // namespace

// static
void ViewsTextServicesContextMenuChromeos::SetImplFactory(
    ImplFactory impl_factory) {
  GetImplFactory() = std::move(impl_factory);
}

ViewsTextServicesContextMenuChromeos::ViewsTextServicesContextMenuChromeos(
    ui::SimpleMenuModel* menu,
    Textfield* client) {
  auto& impl_factory = GetImplFactory();

  // In unit tests, `impl_factory` may not be set. Use
  // `ViewTextServicesContextMenuBase` in that case.
  if (impl_factory) {
    impl_ = impl_factory.Run(menu, client);
  } else {
    impl_ = std::make_unique<ViewsTextServicesContextMenuBase>(menu, client);
  }
}

ViewsTextServicesContextMenuChromeos::~ViewsTextServicesContextMenuChromeos() =
    default;

bool ViewsTextServicesContextMenuChromeos::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return impl_->GetAcceleratorForCommandId(command_id, accelerator);
}

bool ViewsTextServicesContextMenuChromeos::IsCommandIdChecked(
    int command_id) const {
  return impl_->IsCommandIdChecked(command_id);
}

bool ViewsTextServicesContextMenuChromeos::IsCommandIdEnabled(
    int command_id) const {
  return impl_->IsCommandIdEnabled(command_id);
}

void ViewsTextServicesContextMenuChromeos::ExecuteCommand(int command_id,
                                                          int event_flags) {
  return impl_->ExecuteCommand(command_id, event_flags);
}

bool ViewsTextServicesContextMenuChromeos::SupportsCommand(
    int command_id) const {
  return impl_->SupportsCommand(command_id);
}

// static
std::unique_ptr<ViewsTextServicesContextMenu>
ViewsTextServicesContextMenu::Create(ui::SimpleMenuModel* menu,
                                     Textfield* client) {
  return std::make_unique<ViewsTextServicesContextMenuChromeos>(menu, client);
}

}  // namespace views

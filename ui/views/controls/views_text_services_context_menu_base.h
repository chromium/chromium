// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_BASE_H_
#define UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_BASE_H_

#include "build/build_config.h"
#include "ui/views/controls/views_text_services_context_menu.h"

namespace views {

// This base class is used to add and handle text service items in the textfield
// context menu. Specific platforms may subclass and add additional items.
class ViewsTextServicesContextMenuBase : public ViewsTextServicesContextMenu {
 public:
  ViewsTextServicesContextMenuBase(ui::SimpleMenuModel* menu,
                                   Textfield* client);
  ViewsTextServicesContextMenuBase(const ViewsTextServicesContextMenuBase&) =
      delete;
  ViewsTextServicesContextMenuBase& operator=(
      const ViewsTextServicesContextMenuBase&) = delete;
  ~ViewsTextServicesContextMenuBase() override;

  // ViewsTextServicesContextMenu:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool SupportsCommand(int command_id) const override;

 protected:
#if defined(OS_APPLE)
  Textfield* client() { return client_; }
  const Textfield* client() const { return client_; }
#endif

 private:
  // The view associated with the menu. Weak. Owns |this|.
  Textfield* const client_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_BASE_H_

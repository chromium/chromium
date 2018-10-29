// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_BASE_H_
#define UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_BASE_H_

#include "base/macros.h"
#include "ui/views/controls/views_text_services_context_menu.h"

namespace views {

// This base class is used to add and handle text service items in the text
// context menu. Specific platforms may subclass and add additional items.
class ViewsTextServicesContextMenuBase : public ViewsTextServicesContextMenu {
 public:
  ViewsTextServicesContextMenuBase(ui::SimpleMenuModel* menu,
                                   Textfield* client);
  ~ViewsTextServicesContextMenuBase() override;

  // Returns true if the given |command_id| is handled by the menu.
  bool SupportsCommand(int command_id) const override;

  // ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // Methods associated with SimpleMenuModel::Delegate.
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id) override;

 protected:
  Textfield* client() const { return client_; }

 private:
  // The view associated with the menu. Weak. Owns |this|.
  Textfield* client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ViewsTextServicesContextMenuBase);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_BASE_H_

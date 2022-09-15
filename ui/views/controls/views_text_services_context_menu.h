// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_H_
#define UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_H_

#include <memory>

#include "ui/base/models/simple_menu_model.h"

namespace views {

class Textfield;

// This class is used to add and handle text service items in the text context
// menu.
class ViewsTextServicesContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  // Creates a platform-specific ViewsTextServicesContextMenu object.
  static std::unique_ptr<ViewsTextServicesContextMenu> Create(
      ui::SimpleMenuModel* menu,
      Textfield* textfield);

  // Returns true if the given |command_id| is handled by the menu.
  virtual bool SupportsCommand(int command_id) const = 0;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_H_

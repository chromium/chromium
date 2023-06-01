// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "base/no_destructor.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"

namespace views {

MenuConfig::MenuConfig() {
  Init();
  InitCR2023();
}

MenuConfig::~MenuConfig() = default;

int MenuConfig::CornerRadiusForMenu(const MenuController* controller) const {
  if (controller && controller->use_ash_system_ui_layout()) {
    return controller->rounded_corners().has_value() ? 0
                                                     : touchable_corner_radius;
  }

  if (controller && (controller->IsCombobox() ||
                     (!use_bubble_border && controller->IsContextMenu()))) {
    return auxiliary_corner_radius;
  }
  return corner_radius;
}

bool MenuConfig::ShouldShowAcceleratorText(const MenuItemView* item,
                                           std::u16string* text) const {
  if (!show_accelerators || !item->GetDelegate() || !item->GetCommand())
    return false;
  ui::Accelerator accelerator;
  if (!item->GetDelegate()->GetAccelerator(item->GetCommand(), &accelerator))
    return false;
  if (item->GetMenuController() && item->GetMenuController()->IsContextMenu() &&
      !show_context_menu_accelerators) {
    return false;
  }
  *text = accelerator.GetShortcutText();
  return true;
}

void MenuConfig::InitCR2023() {
  if (!features::IsChromeRefresh2023()) {
    return;
  }
  // CR2023 menu metrics
  align_arrow_and_shortcut = true;
  separator_height = 17;
  separator_left_margin = 12;
  separator_right_margin = 12;
  item_top_margin = 6;
  item_bottom_margin = 6;
  item_horizontal_border_padding = 12;
}

// static
const MenuConfig& MenuConfig::instance() {
  static base::NoDestructor<MenuConfig> instance;
  return *instance;
}

}  // namespace views

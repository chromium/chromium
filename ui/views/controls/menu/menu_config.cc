// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "base/debug/dump_without_crashing.h"
#include "base/no_destructor.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace views {

MenuConfig::MenuConfig() {
  InitCommon();
  InitPlatform();
}

MenuConfig::~MenuConfig() = default;

int MenuConfig::CornerRadiusForMenu(const MenuController* controller) const {
#if BUILDFLAG(IS_OZONE)
  if (!ui::OzonePlatform::GetInstance()->IsWindowCompositingSupported()) {
    return 0;
  }
#endif
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
  if (!show_accelerators || !item->GetDelegate() || !item->GetCommand()) {
    return false;
  }
  if (item->GetDelegate()->IsTearingDown()) {
    // The delegate should not be used once teardown has begun. Remove this once
    // crash root cause has been determined (crbug.com/1283454).
    base::debug::DumpWithoutCrashing();
    return false;
  }
  ui::Accelerator accelerator;
  if (!item->GetDelegate()->GetAccelerator(item->GetCommand(), &accelerator)) {
    return false;
  }
  if (item->GetMenuController() && item->GetMenuController()->IsContextMenu() &&
      !show_context_menu_accelerators) {
    return false;
  }
  *text = accelerator.GetShortcutText();
  return true;
}

void MenuConfig::InitCommon() {
  context_menu_font_list = font_list = TypographyProvider::Get().GetFont(
      style::CONTEXT_MENU, style::STYLE_BODY_3);
  reserve_dedicated_arrow_column = false;
  menu_horizontal_border_size = 0;
  submenu_horizontal_overlap = 0;
  item_vertical_margin = 6;
  item_horizontal_border_padding = 12;
  arrow_size = 16;
  separator_height = 17;
  separator_spacing_height = 4;
  use_outer_border = false;
}

// static
const MenuConfig& MenuConfig::instance() {
  static base::NoDestructor<MenuConfig> instance;
  return *instance;
}

}  // namespace views

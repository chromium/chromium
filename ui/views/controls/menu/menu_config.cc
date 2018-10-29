// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "base/macros.h"
#include "base/no_destructor.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_image_util.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/round_rect_painter.h"

namespace views {

MenuConfig::MenuConfig()
    : arrow_color(SK_ColorBLACK),
      menu_vertical_border_size(4),
      menu_horizontal_border_size(views::RoundRectPainter::kBorderWidth),
      submenu_horizontal_inset(3),
      item_top_margin(4),
      item_bottom_margin(3),
      item_no_icon_top_margin(4),
      item_no_icon_bottom_margin(4),
      minimum_text_item_height(0),
      minimum_container_item_height(0),
      minimum_menu_width(0),
      // TODO(ftirelo): Paddings should come from the layout provider, once
      //                Harmony is the default behavior.
      item_horizontal_padding(8),
      touchable_item_horizontal_padding(16),
      label_to_arrow_padding(8),
      arrow_to_edge_padding(5),
      touchable_icon_size(20),
      touchable_icon_color(SkColorSetRGB(0x5F, 0x63, 0x60)),
      check_width(kMenuCheckSize),
      check_height(kMenuCheckSize),
      arrow_width(kSubmenuArrowSize),
      separator_height(11),
      double_separator_height(18),
      separator_upper_height(3),
      separator_lower_height(4),
      separator_spacing_height(3),
      separator_thickness(1),
      double_separator_thickness(2),
      show_mnemonics(false),
      use_mnemonics(true),
      scroll_arrow_height(3),
      item_min_height(0),
      actionable_submenu_arrow_to_edge_padding(14),
      actionable_submenu_width(37),
      actionable_submenu_vertical_separator_height(18),
      actionable_submenu_vertical_separator_width(1),
      show_accelerators(true),
      always_use_icon_to_label_padding(false),
      align_arrow_and_shortcut(false),
      offset_context_menus(false),
      use_outer_border(true),
      icons_in_label(false),
      check_selected_combobox_item(false),
      show_delay(400),
      corner_radius(0),
      auxiliary_corner_radius(0),
      touchable_corner_radius(8),
      touchable_anchor_offset(8),
      touchable_menu_height(36),
      touchable_menu_width(256),
      touchable_menu_shadow_elevation(12),
      vertical_touchable_menu_item_padding(8),
      padded_separator_left_margin(64),
      arrow_key_selection_wraps(true),
      show_context_menu_accelerators(true),
      all_menus_use_prefix_selection(false) {
  Init();
}

MenuConfig::~MenuConfig() {}

int MenuConfig::CornerRadiusForMenu(const MenuController* controller) const {
  if (controller && controller->use_touchable_layout())
    return touchable_corner_radius;
  if (controller && (controller->is_combobox() || controller->IsContextMenu()))
    return auxiliary_corner_radius;
  return corner_radius;
}

bool MenuConfig::ShouldShowAcceleratorText(const MenuItemView* item,
                                           base::string16* text) const {
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

// static
const MenuConfig& MenuConfig::instance() {
  static base::NoDestructor<MenuConfig> instance;
  return *instance;
}

}  // namespace views

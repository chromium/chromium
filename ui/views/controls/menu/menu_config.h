// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/views/views_export.h"

namespace views {

class MenuController;
class MenuItemView;

// Layout type information for menu items. Use the instance() method to obtain
// the MenuConfig for the current platform.
struct VIEWS_EXPORT MenuConfig {
  MenuConfig();
  ~MenuConfig();

  static const MenuConfig& instance();

  // Helper methods to simplify access to MenuConfig:
  // Returns the appropriate corner radius for the menu controlled by
  // |controller|, or the default corner radius if |controller| is nullptr.
  int CornerRadiusForMenu(const MenuController* controller) const;

  // Returns whether |item_view| should show accelerator text. If so, returns
  // the text to show.
  bool ShouldShowAcceleratorText(const MenuItemView* item_view,
                                 base::string16* text) const;

  // Font list used by menus.
  gfx::FontList font_list;

  // Color for the arrow to scroll bookmarks.
  SkColor arrow_color;

  // Menu border sizes. The vertical border size does not apply to menus with
  // rounded corners - those menus always use the corner radius as the vertical
  // border size.
  int menu_vertical_border_size;
  int menu_horizontal_border_size;

  // Submenu horizontal inset with parent menu. This is the horizontal overlap
  // between the submenu and its parent menu, not including the borders of
  // submenu and parent menu.
  int submenu_horizontal_inset;

  // Margins between the top of the item and the label.
  int item_top_margin;

  // Margins between the bottom of the item and the label.
  int item_bottom_margin;

  // Margins used if the menu doesn't have icons.
  int item_no_icon_top_margin;
  int item_no_icon_bottom_margin;

  // Minimum dimensions used for entire items. If these are nonzero, they
  // override the vertical margin constants given above - the item's text and
  // icon are vertically centered within these heights.
  int minimum_text_item_height;
  int minimum_container_item_height;
  int minimum_menu_width;

  // Horizontal padding between components in a menu item.
  int item_horizontal_padding;

  // Horizontal padding between components in a touchable menu item.
  int touchable_item_horizontal_padding;

  // Padding between the label and submenu arrow.
  int label_to_arrow_padding;

  // Padding between the arrow and the edge.
  int arrow_to_edge_padding;

  // The icon size used for icons in touchable menu items.
  int touchable_icon_size;

  // The color used for icons in touchable menu items.
  SkColor touchable_icon_color;

  // The space reserved for the check. The actual size of the image may be
  // different.
  int check_width;
  int check_height;

  // The horizontal space reserved for submenu arrow. The actual width of the
  // image may be different.
  int arrow_width;

  // Height of a normal separator (ui::NORMAL_SEPARATOR).
  int separator_height;

  // Height of a double separator (ui::DOUBLE_SEPARATOR).
  int double_separator_height;

  // Height of a ui::UPPER_SEPARATOR.
  int separator_upper_height;

  // Height of a ui::LOWER_SEPARATOR.
  int separator_lower_height;

  // Height of a ui::SPACING_SEPARATOR.
  int separator_spacing_height;

  // Thickness of the drawn separator line in pixels.
  int separator_thickness;

  // Thickness of the drawn separator line in pixels for double separator.
  int double_separator_thickness;

  // Are mnemonics shown?
  bool show_mnemonics;

  // Are mnemonics used to activate items?
  bool use_mnemonics;

  // Height of the scroll arrow.
  int scroll_arrow_height;

  // Minimum height of menu item.
  int item_min_height;

  // Edge padding for an actionable submenu arrow.
  int actionable_submenu_arrow_to_edge_padding;

  // Width of the submenu in an actionable submenu.
  int actionable_submenu_width;

  // The height of the vertical separator used in an actionable submenu.
  int actionable_submenu_vertical_separator_height;

  // The width of the vertical separator used in an actionable submenu.
  int actionable_submenu_vertical_separator_width;

  // Whether the keyboard accelerators are visible.
  bool show_accelerators;

  // True if icon to label padding is always added with or without icon.
  bool always_use_icon_to_label_padding;

  // True if submenu arrow and shortcut right edge should be aligned.
  bool align_arrow_and_shortcut;

  // True if the context menu's should be offset from the cursor position.
  bool offset_context_menus;

  // True if the scroll container should add a border stroke around the menu.
  bool use_outer_border;

  // True if the icon is part of the label rather than in its own column.
  bool icons_in_label;

  // True if a combobox menu should put a checkmark next to the selected item.
  bool check_selected_combobox_item;

  // Delay, in ms, between when menus are selected or moused over and the menu
  // appears.
  int show_delay;

  // Radius of the rounded corners of the menu border. Must be >= 0.
  int corner_radius;

  // Radius of "auxiliary" rounded corners - comboboxes and context menus.
  // Must be >= 0.
  int auxiliary_corner_radius;

  // Radius of the rounded corners of the touchable menu border
  int touchable_corner_radius;

  // Anchor offset for touchable menus created by a touch event.
  int touchable_anchor_offset;

  // Height of child MenuItemViews for touchable menus.
  int touchable_menu_height;

  // Width of touchable menus.
  int touchable_menu_width;

  // Shadow elevation of touchable menus.
  int touchable_menu_shadow_elevation;

  // Vertical padding for touchable menus.
  int vertical_touchable_menu_item_padding;

  // Left margin of padded separator (ui::PADDED_SEPARATOR).
  int padded_separator_left_margin;

  // Whether arrow keys should wrap around the end of the menu when selecting.
  bool arrow_key_selection_wraps;

  // Whether to show accelerators in context menus.
  bool show_context_menu_accelerators;

  // Whether all types of menus use prefix selection for items.
  bool all_menus_use_prefix_selection;

 private:
  // Configures a MenuConfig as appropriate for the current platform.
  void Init();
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_

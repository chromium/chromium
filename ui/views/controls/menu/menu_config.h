// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/menu/menu_image_util.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/round_rect_painter.h"
#include "ui/views/views_export.h"

namespace views {

class MenuController;
class MenuItemView;

// Layout type information for menu items. Use the instance() method to obtain
// the MenuConfig for the current platform.
struct VIEWS_EXPORT MenuConfig {
  MenuConfig();
  ~MenuConfig();

  // Menus are the only place using kGroupingPropertyKey, so any value (other
  // than 0) is fine.
  static constexpr int kMenuControllerGroupingId = 1001;

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

  // Menu border sizes. The vertical border size does not apply to menus with
  // rounded corners - those menus always use the corner radius as the vertical
  // border size.
  int menu_vertical_border_size = 4;
  int menu_horizontal_border_size = views::RoundRectPainter::kBorderWidth;

  // Submenu horizontal inset with parent menu. This is the horizontal overlap
  // between the submenu and its parent menu, not including the borders of
  // submenu and parent menu.
  int submenu_horizontal_inset = 3;

  // Margins between the top of the item and the label.
  int item_top_margin = 4;

  // Margins between the bottom of the item and the label.
  int item_bottom_margin = 3;

  // Margins used if the menu doesn't have icons.
  int item_no_icon_top_margin = 4;
  int item_no_icon_bottom_margin = 4;

  // Minimum dimensions used for entire items. If these are nonzero, they
  // override the vertical margin constants given above - the item's text and
  // icon are vertically centered within these heights.
  int minimum_text_item_height = 0;
  int minimum_container_item_height = 0;
  int minimum_menu_width = 0;

  // TODO(ftirelo): Paddings should come from the layout provider, once Harmony
  // is the default behavior.

  // Horizontal padding between components in a menu item.
  int item_horizontal_padding = 8;

  // Horizontal padding between components in a touchable menu item.
  int touchable_item_horizontal_padding = 16;

  // Padding between the label and submenu arrow.
  int label_to_arrow_padding = 8;

  // Padding between the arrow and the edge.
  int arrow_to_edge_padding = 5;

  // The space reserved for the check. The actual size of the image may be
  // different.
  int check_width = kMenuCheckSize;
  int check_height = kMenuCheckSize;

  // The horizontal space reserved for submenu arrow. The actual width of the
  // image may be different.
  int arrow_width = kSubmenuArrowSize;

  // Height of a normal separator (ui::NORMAL_SEPARATOR).
  int separator_height = 11;

  // Height of a double separator (ui::DOUBLE_SEPARATOR).
  int double_separator_height = 18;

  // Height of a ui::UPPER_SEPARATOR.
  int separator_upper_height = 3;

  // Height of a ui::LOWER_SEPARATOR.
  int separator_lower_height = 4;

  // Height of a ui::SPACING_SEPARATOR.
  int separator_spacing_height = 3;

  // Thickness of the drawn separator line in pixels.
  int separator_thickness = 1;

  // Thickness of the drawn separator line in pixels for double separator.
  int double_separator_thickness = 2;

  // Are mnemonics shown?
  bool show_mnemonics = false;

  // Are mnemonics used to activate items?
  bool use_mnemonics = true;

  // Height of the scroll arrow.
  int scroll_arrow_height = 3;

  // Minimum height of menu item.
  int item_min_height = 0;

  // Edge padding for an actionable submenu arrow.
  int actionable_submenu_arrow_to_edge_padding = 14;

  // Width of the submenu in an actionable submenu.
  int actionable_submenu_width = 37;

  // The height of the vertical separator used in an actionable submenu.
  int actionable_submenu_vertical_separator_height = 18;

  // The width of the vertical separator used in an actionable submenu.
  int actionable_submenu_vertical_separator_width = 1;

  // Whether the keyboard accelerators are visible.
  bool show_accelerators = true;

  // True if icon to label padding is always added with or without icon.
  bool always_use_icon_to_label_padding = false;

  // True if submenu arrow and shortcut right edge should be aligned.
  bool align_arrow_and_shortcut = false;

  // True if the context menu's should be offset from the cursor position.
  bool offset_context_menus = false;

  // True if the scroll container should add a border stroke around the menu.
  bool use_outer_border = true;

  // True if the icon is part of the label rather than in its own column.
  bool icons_in_label = false;

  // True if a combobox menu should put a checkmark next to the selected item.
  bool check_selected_combobox_item = false;

  // Delay, in ms, between when menus are selected or moused over and the menu
  // appears.
  int show_delay = 400;

  // Radius of the rounded corners of the menu border. Must be >= 0.
  int corner_radius =
      LayoutProvider::Get()->GetCornerRadiusMetric(EMPHASIS_NONE);

  // Radius of "auxiliary" rounded corners - comboboxes and context menus.
  // Must be >= 0.
  int auxiliary_corner_radius =
      LayoutProvider::Get()->GetCornerRadiusMetric(EMPHASIS_NONE);

  // Radius of the rounded corners of the touchable menu border
  int touchable_corner_radius =
      LayoutProvider::Get()->GetCornerRadiusMetric(EMPHASIS_HIGH);

  // Anchor offset for touchable menus created by a touch event.
  int touchable_anchor_offset = 8;

  // Height of child MenuItemViews for touchable menus.
  int touchable_menu_height = 36;

  // Width of touchable menus.
  int touchable_menu_width = 256;

  // Shadow elevation of touchable menus.
  int touchable_menu_shadow_elevation = 12;

  // Vertical padding for touchable menus.
  int vertical_touchable_menu_item_padding = 8;

  // Left margin of padded separator (ui::PADDED_SEPARATOR).
  int padded_separator_left_margin = 64;

  // Whether arrow keys should wrap around the end of the menu when selecting.
  bool arrow_key_selection_wraps = true;

  // Whether to show accelerators in context menus.
  bool show_context_menu_accelerators = true;

  // Whether all types of menus use prefix selection for items.
  bool all_menus_use_prefix_selection = false;

  // Margins for footnotes (HIGHLIGHTED item at the end of a menu).
  int footnote_vertical_margin = 11;

 private:
  // Configures a MenuConfig as appropriate for the current platform.
  void Init();
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_

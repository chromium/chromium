// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_

#include <optional>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/round_rect_painter.h"
#include "ui/views/views_export.h"

namespace views {

class MenuController;
class MenuItemView;

constexpr int kMenuCheckSize = 16;

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
                                 std::u16string* text) const;

  // Font lists used by menus.
  gfx::FontList font_list;
  gfx::FontList context_menu_font_list;

  // Menu border sizes. Menus with rounded corners use
  // `rounded_menu_vertical_border_size` if set and fall back to the corner
  // radius otherwise.
  int nonrounded_menu_vertical_border_size = 4;
  std::optional<int> rounded_menu_vertical_border_size;
  int menu_horizontal_border_size = views::RoundRectPainter::kBorderWidth;

  // The horizontal overlap between the submenu and its parent menu item.
  int submenu_horizontal_overlap = 3;

  // Margins between the item top/bottom and its contents.
  int item_vertical_margin = 4;

  // Margins between the item top/bottom and its contents for ash system ui
  // layout.
  int ash_item_vertical_margin = 4;

  // Minimum dimensions used for entire items. If these are nonzero, they
  // override the vertical margin constants given above - the item's text and
  // icon are vertically centered within these heights.
  int minimum_text_item_height = 0;
  int minimum_container_item_height = 0;

  // TODO(ftirelo): Paddings should come from the layout provider, once Harmony
  // is the default behavior.

  // Horizontal padding between components in a menu item.
  int item_horizontal_padding = 8;

  // Horizontal padding between components in a touchable menu item.
  int touchable_item_horizontal_padding = 16;

  // Additional padding between the item left/right and its contents. Note that
  // the final padding will also include `item_horizontal_padding`.
  int item_horizontal_border_padding = 0;

  // Horizontal border padding in a menu item for ash system ui layout.
  int ash_item_horizontal_border_padding = 0;

  // Size (width and height) of arrow bounding box.
  int arrow_size = 8;

  // Padding between the arrow and the edge.
  int arrow_to_edge_padding = 8;

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

  // Horizontal border padding of a separator.
  int separator_horizontal_border_padding = 0;

  // Padding, if any, between successive menu items. This is not applied below
  // LOWER_SEPARATORs or above UPPER_SEPARATORs, since these are meant to be
  // flush with the respective adjacent items.
  int between_item_vertical_padding = 0;

  // Are mnemonics shown?
  bool show_mnemonics = false;

  // Are mnemonics used to activate items?
  bool use_mnemonics = true;

  // Height of the scroll arrow.
  int scroll_arrow_height = 3;

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

  // True if submenu arrows should get their own column, separate from minor
  // text.
  bool reserve_dedicated_arrow_column = true;

  // True if the scroll container should add a border stroke around the menu.
  bool use_outer_border = true;

  // True if the icon is part of the label rather than in its own column.
  bool icons_in_label = false;

  // Spacing between icon and main label.
  int icon_label_spacing = LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL);

  // Menus lay out as if some items have checkmarks, even if none do.
  bool always_reserve_check_region = false;

  // True if a combobox menu should put a checkmark next to the selected item.
  bool check_selected_combobox_item = false;

  // Delay, in ms, between when menus are selected or moused over and the menu
  // appears.
  int show_delay = 400;

  // Radius of the rounded corners of the menu border. Must be >= 0.
  int corner_radius = LayoutProvider::Get()->GetCornerRadiusMetric(
      ShapeContextTokens::kMenuRadius);

  // Radius of "auxiliary" rounded corners - comboboxes and context menus.
  // Must be >= 0.
  int auxiliary_corner_radius = LayoutProvider::Get()->GetCornerRadiusMetric(
      ShapeContextTokens::kMenuAuxRadius);

  // Radius of the rounded corners of the touchable menu border
  int touchable_corner_radius = LayoutProvider::Get()->GetCornerRadiusMetric(
      ShapeContextTokens::kMenuTouchRadius);

  // Radius of selection background on menu items.
  int item_corner_radius = 0;

  // Anchor offset for touchable menus created by a touch event.
  int touchable_anchor_offset = 8;

  // Height of child MenuItemViews for touchable menus.
  int touchable_menu_height = 36;

  // Minimum width of touchable menus.
  int touchable_menu_min_width = 256;

  // Maximum width of touchable menus.
  int touchable_menu_max_width = 352;

  // Shadow elevation of bubble menus.
  int bubble_menu_shadow_elevation = 12;

  // Shadow elevation of bubble submenus.
  int bubble_submenu_shadow_elevation = 16;

  // Vertical padding for touchable menus.
  int vertical_touchable_menu_item_padding = 8;

  // Padding at the start of a padded separator (ui::PADDED_SEPARATOR).
  int padded_separator_start_padding = 64;

  // Whether arrow keys should wrap around the end of the menu when selecting.
  bool arrow_key_selection_wraps = true;

  // Whether to show accelerators in context menus.
  bool show_context_menu_accelerators = true;

  // Whether all types of menus use prefix selection for items.
  bool all_menus_use_prefix_selection = false;

  // Margins for footnotes (HIGHLIGHTED item at the end of a menu).
  int footnote_vertical_margin = 11;

  // Should use a bubble border for menus.
  bool use_bubble_border = false;

 private:
  // Set configuration as appropriate for the current platform. Called after
  // InitCommon to make sure that fonts are correct or that other settings are
  // overridden from their defaults.
  void InitPlatform();

  // Set default configuration that is shared by all platforms.
  void InitCommon();
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_

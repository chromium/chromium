// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"

namespace {

void InitMaterialMenuConfig(views::MenuConfig* config) {
  // These config parameters are from https://crbug.com/829347 and the spec
  // images linked from that bug.
  config->menu_horizontal_border_size = 0;
  config->submenu_horizontal_inset = 0;
  config->minimum_text_item_height = 28;
  config->minimum_container_item_height = 40;
  config->minimum_menu_width = 320;
  config->label_to_arrow_padding = 0;
  config->arrow_to_edge_padding = 16;
  config->check_width = 16;
  config->check_height = 16;
  config->arrow_width = 8;
  config->separator_height = 9;
  config->separator_lower_height = 4;
  config->separator_upper_height = 4;
  config->separator_spacing_height = 5;
  config->separator_thickness = 1;
  config->align_arrow_and_shortcut = true;
  config->use_outer_border = false;
  config->icons_in_label = true;
  config->corner_radius = 8;
  config->auxiliary_corner_radius = 4;
  config->item_top_margin = 4;
  config->item_bottom_margin = 4;
}

}  // namespace

namespace views {

void MenuConfig::Init() {
  font_list = gfx::FontList(gfx::Font([NSFont menuFontOfSize:0.0]));
  check_selected_combobox_item = true;
  arrow_key_selection_wraps = false;
  use_mnemonics = false;
  show_context_menu_accelerators = false;
  all_menus_use_prefix_selection = true;
  InitMaterialMenuConfig(this);
}

}  // namespace views

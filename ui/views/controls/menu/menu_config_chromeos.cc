// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "ui/base/ui_base_features.h"

namespace views {

void MenuConfig::Init() {
  if (!features::IsChromeRefresh2023()) {
    arrow_to_edge_padding = 21;              // Undesirable in CR2023.
    corner_radius = 2;                       // Overridden in CR2023.
    minimum_text_item_height = 29;           // Undesirable in CR2023.
    minimum_container_item_height = 29;      // Undesirable in CR2023.
    reserve_dedicated_arrow_column = false;  // Default in CR2023.
    menu_horizontal_border_size = 0;         // Default in CR2023.
    separator_lower_height = 8;              // Unused in CR2023.
    separator_spacing_height = 7;            // Overridden in CR2023.
    separator_upper_height = 8;              // Unused in CR2023.
    submenu_horizontal_overlap = 1;          // Overridden in CR2023.
    use_outer_border = false;                // Default in CR2023.
  }
}

void MenuConfig::InitPlatformCR2023() {
  context_menu_font_list = font_list;
}

}  // namespace views

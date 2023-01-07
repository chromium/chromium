// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include "build/chromeos_buildflags.h"
#include "ui/views/controls/menu/menu_image_util.h"

namespace views {

void MenuConfig::Init() {
  submenu_horizontal_inset = 1;
  arrow_to_edge_padding = 21;
  gfx::ImageSkia check = GetMenuCheckImage(false);
  check_height = check.height();
  item_min_height = 29;
  separator_spacing_height = 7;
  separator_lower_height = 8;
  separator_upper_height = 8;
  always_use_icon_to_label_padding = true;
  align_arrow_and_shortcut = true;
  offset_context_menus = true;
  corner_radius = 2;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug/1129012): Change to false once pop-up menus have shadows.
  use_outer_border = true;
#else
  // In Ash, the border is provided by the shadow.
  use_outer_border = false;
#endif
}

}  // namespace views

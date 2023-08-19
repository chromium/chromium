// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include <windows.h>  // Must come before other Windows system headers.

#include <Vssym32.h>

#include "base/metrics/histogram_macros.h"
#include "base/win/windows_version.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/system_fonts_win.h"

namespace views {

void MenuConfig::Init() {
  context_menu_font_list = font_list =
      gfx::FontList(gfx::win::GetSystemFont(gfx::win::SystemFont::kMenu));

  BOOL show_cues;
  show_mnemonics =
      (SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &show_cues, 0) &&
       show_cues == TRUE);

  SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &show_delay, 0);

  bool is_win11 = base::win::GetVersion() >= base::win::Version::WIN11;
  bool is_refresh = features::IsChromeRefresh2023();
  UMA_HISTOGRAM_BOOLEAN("Windows.Menu.Win11Style", is_win11 && !is_refresh);
  separator_upper_height = 5;
  separator_lower_height = 7;

  if (!is_refresh) {
    if (is_win11) {
      corner_radius = 8;
      menu_horizontal_border_size = 4;
      submenu_horizontal_overlap = 1;
      rounded_menu_vertical_border_size = 4;
      item_horizontal_padding = 12;
      between_item_vertical_padding = 2;
      separator_height = 1;
      separator_upper_height = 1;
      separator_lower_height = 1;
      item_corner_radius = 4;
    } else {
      menu_horizontal_border_size = 3;
      nonrounded_menu_vertical_border_size = 3;
      item_vertical_margin = 3;
      item_horizontal_border_padding = -2;
      icon_label_spacing = 10;
      always_reserve_check_region = true;
      separator_height = 7;
      separator_upper_height = 5;
      separator_lower_height = 5;
    }
  }

  use_bubble_border = corner_radius > 0;
}

void MenuConfig::InitPlatformCR2023() {
  // No platform specific CR2023 initialization needed.
  // context_menu_font_list will use the default Windows system menu font.
}

}  // namespace views

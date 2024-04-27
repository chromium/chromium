// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include <windows.h>  // Must come before other Windows system headers.

#include <Vssym32.h>

#include "ui/gfx/system_fonts_win.h"

namespace views {

void MenuConfig::InitPlatform() {
  context_menu_font_list = font_list =
      gfx::FontList(gfx::win::GetSystemFont(gfx::win::SystemFont::kMenu));

  BOOL show_cues;
  show_mnemonics =
      (SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &show_cues, 0) &&
       show_cues == TRUE);

  SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &show_delay, 0);
  separator_upper_height = 5;
  separator_lower_height = 7;

  use_bubble_border = corner_radius > 0;
}

}  // namespace views

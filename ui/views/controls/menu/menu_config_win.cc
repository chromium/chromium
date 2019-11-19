// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include <windows.h>
#include <uxtheme.h>
#include <Vssym32.h>

#include "base/logging.h"
#include "base/win/scoped_gdi_object.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/system_fonts_win.h"
#include "ui/native_theme/native_theme_win.h"

using ui::NativeTheme;

namespace views {

void MenuConfig::Init() {
  arrow_color = color_utils::GetSysSkColor(COLOR_MENUTEXT);
  font_list =
      gfx::FontList(gfx::win::GetSystemFont(gfx::win::SystemFont::kMenu));

  NativeTheme::ExtraParams extra;
  gfx::Size arrow_size = NativeTheme::GetInstanceForNativeUi()->GetPartSize(
      NativeTheme::kMenuPopupArrow, NativeTheme::kNormal, extra);
  if (!arrow_size.IsEmpty()) {
    arrow_width = arrow_size.width();
  } else {
    // Sadly I didn't see a specify metrics for this.
    arrow_width = GetSystemMetrics(SM_CXMENUCHECK);
  }

  BOOL show_cues;
  show_mnemonics =
      (SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &show_cues, 0) &&
       show_cues == TRUE);

  SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &show_delay, 0);

  separator_upper_height = 5;
  separator_lower_height = 7;
}

}  // namespace views

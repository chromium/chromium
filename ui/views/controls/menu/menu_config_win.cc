// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#include <windows.h>  // Must come before other Windows system headers.

#include <Vssym32.h>
#include <uxtheme.h>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/windows_version.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/system_fonts_win.h"
#include "ui/native_theme/native_theme_win.h"

using ui::NativeTheme;

namespace views {

void MenuConfig::Init() {
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

  win11_style_menus = base::win::GetVersion() >= base::win::Version::WIN11;
  UMA_HISTOGRAM_BOOLEAN("Windows.Menu.Win11Style", win11_style_menus);
  separator_upper_height = 5;
  separator_lower_height = 7;

  if (win11_style_menus)
    corner_radius = 8;
}

}  // namespace views

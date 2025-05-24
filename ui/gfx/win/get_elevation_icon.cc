// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/get_elevation_icon.h"

#include <windows.h>

#include <shellapi.h>

#include "base/win/com_init_util.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/icon_util.h"

namespace gfx::win {

SkBitmap GetElevationIcon() {
  base::win::AssertComInitialized();

  if (!base::win::UserAccountControlIsEnabled()) {
    return {};
  }

  SHSTOCKICONINFO icon_info = {.cbSize = sizeof(SHSTOCKICONINFO)};
  if (FAILED(::SHGetStockIconInfo(SIID_SHIELD, SHGSI_ICON | SHGSI_SMALLICON,
                                  &icon_info))) {
    return {};
  }
  base::win::ScopedGDIObject<HICON> shield_icon(icon_info.hIcon);

  return IconUtil::CreateSkBitmapFromHICON(
      shield_icon.get(), gfx::Size(::GetSystemMetrics(SM_CXSMICON),
                                   ::GetSystemMetrics(SM_CYSMICON)));
}

}  // namespace gfx::win

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_UTIL_WIN_H_
#define UI_GFX_FONT_UTIL_WIN_H_

#include "base/win/registry.h"

namespace gfx {

class FontUtilWin {
 public:
  static base::win::RegKey GetTextSettingsRegistryKey(REGSAM access = KEY_READ);
  static float ClampContrast(float value);
  static float ClampGamma(float value);
  static float GetContrastFromRegistry();
  static float GetGammaFromRegistry();
  static float TextGammaContrast();
};

}  // namespace gfx

#endif  // UI_GFX_FONT_UTIL_WIN_H_

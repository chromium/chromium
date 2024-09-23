// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_utils.h"

#include <string_view>

namespace ui {

std::string_view NativeThemeColorSchemeName(
    NativeTheme::ColorScheme color_scheme) {
  switch (color_scheme) {
    case NativeTheme::ColorScheme::kDefault:
      return "kDefault";
    case NativeTheme::ColorScheme::kLight:
      return "kLight";
    case NativeTheme::ColorScheme::kDark:
      return "kDark";
    case NativeTheme::ColorScheme::kPlatformHighContrast:
      return "kPlatformHighContrast";
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid NativeTheme::ColorScheme";
      return "<invalid>";
  }
}

}  // namespace ui

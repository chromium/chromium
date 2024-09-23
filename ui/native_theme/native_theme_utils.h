// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_UTILS_H_
#define UI_NATIVE_THEME_NATIVE_THEME_UTILS_H_

#include <string_view>

#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_export.h"

namespace ui {

// The following functions convert various values to strings intended for
// logging. Do not retain the results for longer than the scope in which these
// functions are called.

// Converts NativeTheme::ColorScheme.
std::string_view NATIVE_THEME_EXPORT
NativeThemeColorSchemeName(NativeTheme::ColorScheme color_scheme);

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_UTILS_H_

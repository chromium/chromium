// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_UTILS_H_
#define UI_COLOR_COLOR_PROVIDER_UTILS_H_

#include <string>

#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_manager.h"

namespace ui {

// The following functions convert various values to strings intended for
// logging. Do not retain the results for longer than the scope in which these
// functions are called.

// Converts the ColorMode.
base::StringPiece COMPONENT_EXPORT(COLOR)
    ColorModeName(ColorProviderManager::ColorMode color_mode);

// Converts the ContrastMode.
base::StringPiece COMPONENT_EXPORT(COLOR)
    ContrastModeName(ColorProviderManager::ContrastMode contrast_mode);

// Converts SystemTheme.
base::StringPiece COMPONENT_EXPORT(COLOR)
    SystemThemeName(ColorProviderManager::SystemTheme system_theme);

// Converts ColorId.
base::StringPiece COMPONENT_EXPORT(COLOR) ColorIdName(ColorId color_id);

// Converts ColorSetId.
base::StringPiece COMPONENT_EXPORT(COLOR)
    ColorSetIdName(ColorSetId color_set_id);

// Converts SkColor to string. Check if color matches a standard color palette
// value and return it as a string. Otherwise return as an rgba(xx, xxx, xxx,
// xxx) string.
std::string COMPONENT_EXPORT(COLOR) SkColorName(SkColor color);

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_UTILS_H_

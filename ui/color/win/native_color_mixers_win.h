// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_WIN_NATIVE_COLOR_MIXERS_WIN_H_
#define UI_COLOR_WIN_NATIVE_COLOR_MIXERS_WIN_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"

namespace ui {

// Returns a map from the Windows-specific native color IDs to their current
// corresponding system colors, as given by the OS `GetSysColor()` function.
COMPONENT_EXPORT(COLOR) base::flat_map<ColorId, SkColor> GetCurrentSysColors();

}  // namespace ui

#endif  // UI_COLOR_WIN_NATIVE_COLOR_MIXERS_WIN_H_

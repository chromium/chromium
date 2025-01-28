// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_WIN_GET_ELEVATION_ICON_H_
#define UI_GFX_WIN_GET_ELEVATION_ICON_H_

#include "base/component_export.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx::win {

// Returns the system's security shield icon for use for UAC prompts, or an
// empty bitmap if UAC is disabled. The icon will be scaled for the current DPI.
COMPONENT_EXPORT(GFX) SkBitmap GetElevationIcon();

}  // namespace gfx::win

#endif  // UI_GFX_WIN_GET_ELEVATION_ICON_H_

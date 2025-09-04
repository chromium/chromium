// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_OVERLAY_SCROLLBAR_CONSTANTS_H_
#define UI_NATIVE_THEME_OVERLAY_SCROLLBAR_CONSTANTS_H_

#include "base/component_export.h"
#include "base/time/time.h"

namespace ui {

inline constexpr int kOverlayScrollbarStrokeWidth = 1;
inline constexpr int kOverlayScrollbarThumbWidthPressed = 10;
inline constexpr float kOverlayScrollbarIdleThicknessScale = 0.4f;

COMPONENT_EXPORT(NATIVE_THEME) base::TimeDelta GetOverlayScrollbarFadeDelay();
COMPONENT_EXPORT(NATIVE_THEME)
base::TimeDelta GetOverlayScrollbarFadeDuration();

}  // namespace ui

#endif  // UI_NATIVE_THEME_OVERLAY_SCROLLBAR_CONSTANTS_H_

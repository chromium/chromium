// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_OVERLAY_SCROLLBAR_CONSTANTS_AURA_H_
#define UI_NATIVE_THEME_OVERLAY_SCROLLBAR_CONSTANTS_AURA_H_

#include "base/time/time.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace ui {

constexpr int kOverlayScrollbarStrokeWidth = 1;
constexpr int kOverlayScrollbarThumbWidthPressed = 10;
constexpr float kOverlayScrollbarIdleThicknessScale = 0.4f;

constexpr base::TimeDelta kOverlayScrollbarFadeDelay = base::Milliseconds(500);
constexpr base::TimeDelta kOverlayScrollbarFadeDuration =
    base::Milliseconds(200);
// TODO(bokan): This is still undetermined. crbug.com/652520.
constexpr base::TimeDelta kOverlayScrollbarThinningDuration =
    base::Milliseconds(200);

// Fluent overlay scrollbar animation times are set to align with the Fluent
// design language.
constexpr base::TimeDelta kFluentOverlayScrollbarFadeDelay =
    base::Milliseconds(750);
constexpr base::TimeDelta kFluentOverlayScrollbarFadeDuration =
    base::Milliseconds(100);
constexpr base::TimeDelta kFluentOverlayScrollbarThinningDuration =
    base::Milliseconds(100);

}  // namespace ui

#endif  // UI_NATIVE_THEME_OVERLAY_SCROLLBAR_CONSTANTS_AURA_H_

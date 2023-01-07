// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_PROGRESS_RING_UTILS_H_
#define UI_VIEWS_CONTROLS_PROGRESS_RING_UTILS_H_

#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

// Helper function that draws a progress ring on `canvas`. The progress ring
// consists a background full ring and a partial ring that indicates the
// progress. `start_angle` and `sweep_angle` are used to indicate the current
// progress of the ring.
VIEWS_EXPORT void DrawProgressRing(gfx::Canvas* canvas,
                                   const SkRect& bounds,
                                   SkColor background_color,
                                   SkColor progress_color,
                                   float stroke_width,
                                   SkScalar start_angle,
                                   SkScalar sweep_angle);

// Helper function that draws a spinning ring on `canvas`. The spinning ring
// consists a background full ring and three arches distributed evenly on the
// ring. `start_angle` is used to indicate the start angle of the first arch.
VIEWS_EXPORT void DrawSpinningRing(gfx::Canvas* canvas,
                                   const SkRect& bounds,
                                   SkColor background_color,
                                   SkColor progress_color,
                                   float stroke_width,
                                   SkScalar start_angle);

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_PROGRESS_RING_UTILS_H_

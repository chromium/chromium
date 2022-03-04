// Copyright 2022 The Chromium Authors. All rights reserved.
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

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_PROGRESS_RING_UTILS_H_

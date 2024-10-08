// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PAINT_THROBBER_H_
#define UI_GFX_PAINT_THROBBER_H_

#include <stdint.h>

#include <optional>

#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class Canvas;
class Rect;

// This struct describes the "spinning" state of a throb animation.
struct GFX_EXPORT ThrobberSpinningState {
  // The start angle of the arc in degrees.
  SkScalar start_angle = 0.f;

  // The sweep angle of the arc in degrees. Positive is clockwise.
  SkScalar sweep_angle = 0.f;
};

// Returns the calculated state for a single frame of the throbber in the
// "spinning", aka Material, state. Note that animation duration is a hardcoded
// value in line with Material design specifications but is cyclic, so the
// specified `elapsed_time` may exceed animation duration.
// The `sweep_keyframe_offset` parameter can be used to begin the animation
// from a different keyframe.
GFX_EXPORT ThrobberSpinningState
CalculateThrobberSpinningState(base::TimeDelta elapsed_time,
                               const int64_t sweep_keyframe_offset = 0);

// Paints a single frame of the throbber in the "spinning", aka Material, state.
// Note that animation duration is a hardcoded value in line with Material
// design specifications but is cyclic, so the specified `elapsed_time` may
// exceed animation duration.
GFX_EXPORT void PaintThrobberSpinning(
    Canvas* canvas,
    const Rect& bounds,
    SkColor color,
    const base::TimeDelta& elapsed_time,
    std::optional<SkScalar> stroke_width = std::nullopt);

// Similar to PaintThrobberSpinner, but starts the spinning animation from the
// second keyframe, where the spinner's arc starts collapsed and then grows.
GFX_EXPORT void PaintThrobberSpinningWithSweepEasedIn(
    Canvas* canvas,
    const Rect& bounds,
    SkColor color,
    const base::TimeDelta& elapsed_time,
    std::optional<SkScalar> stroke_width = std::nullopt);

}  // namespace gfx

#endif  // UI_GFX_PAINT_THROBBER_H_

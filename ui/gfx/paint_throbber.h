// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PAINT_THROBBER_H_
#define UI_GFX_PAINT_THROBBER_H_

#include <stdint.h>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

class Canvas;
class Rect;
class RectF;

// This struct describes the "waiting" mode of a throb animation. It's useful
// for building a "spinning" state animation on top of a previous "waiting"
// mode animation. See PaintThrobberSpinningAfterWaiting.
struct GFX_EXPORT ThrobberWaitingState {
  // The amount of time that was spent in the waiting state.
  base::TimeDelta elapsed_time;

  // The color of the arc in the waiting state.
  SkColor color = SK_ColorTRANSPARENT;

  // An opaque value used to cache calculations made by
  // PaintThrobberSpinningAfterWaiting.
  base::TimeDelta arc_time_offset;
};

// Paints a single frame of the throbber in the "spinning", aka Material, state.
GFX_EXPORT void PaintThrobberSpinning(
    Canvas* canvas,
    const Rect& bounds,
    SkColor color,
    const base::TimeDelta& elapsed_time,
    absl::optional<SkScalar> stroke_width = absl::nullopt);

// Paints a throbber in the "waiting" state. Used when waiting on a network
// response, for example.
GFX_EXPORT void PaintThrobberWaiting(
    Canvas* canvas,
    const Rect& bounds,
    SkColor color,
    const base::TimeDelta& elapsed_time,
    absl::optional<SkScalar> stroke_width = absl::nullopt);

// Paint a throbber in the "spinning" state, smoothly transitioning from a
// previous "waiting" state described by |waiting_state|, which is an in-out
// param.
GFX_EXPORT void PaintThrobberSpinningAfterWaiting(
    Canvas* canvas,
    const Rect& bounds,
    SkColor color,
    const base::TimeDelta& elapsed_time,
    ThrobberWaitingState* waiting_state,
    absl::optional<SkScalar> stroke_width = absl::nullopt);

// Paints a throbber in the "waiting" state (bouncing back and forth). Used when
// waiting on a network response, for example.
GFX_EXPORT void PaintNewThrobberWaiting(Canvas* canvas,
                                        const RectF& throbber_container_bounds,
                                        SkColor color,
                                        const base::TimeDelta& elapsed_time);

}  // namespace gfx

#endif  // UI_GFX_PAINT_THROBBER_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/paint_throbber.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace gfx {

namespace {

// The maximum size of the "spinning" state arc, in degrees.
constexpr int64_t kMaxArcSize = 270;

// The amount of time it takes to grow the "spinning" arc from 0 to 270 degrees.
constexpr auto kArcTime = base::Seconds(2.0 / 3.0);

// The amount of time it takes for the "spinning" throbber to make a full
// rotation.
constexpr auto kRotationTime = base::Milliseconds(1568);

void PaintArc(Canvas* canvas,
              const Rect& bounds,
              SkColor color,
              SkScalar start_angle,
              SkScalar sweep,
              std::optional<SkScalar> stroke_width) {
  if (!stroke_width) {
    // Stroke width depends on size.
    // . For size < 28:          3 - (28 - size) / 16
    // . For 28 <= size:         (8 + size) / 12
    stroke_width = bounds.width() < 28
                       ? 3.0 - SkIntToScalar(28 - bounds.width()) / 16.0
                       : SkIntToScalar(bounds.width() + 8) / 12.0;
  }
  Rect oval = bounds;
  // Inset by half the stroke width to make sure the whole arc is inside
  // the visible rect.
  const int inset = SkScalarCeilToInt(*stroke_width / 2.0);
  oval.Inset(inset);

  SkPath path;
  path.arcTo(RectToSkRect(oval), start_angle, sweep, true);

  cc::PaintFlags flags;
  flags.setColor(color);
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  flags.setStrokeWidth(*stroke_width);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  canvas->DrawPath(path, flags);
}

// This is a Skia port of the MD spinner SVG. The |start_angle| rotation
// here corresponds to the 'rotate' animation.
ThrobberSpinningState CalculateThrobberSpinningStateWithStartAngle(
    base::TimeDelta elapsed_time,
    int64_t start_angle,
    const int64_t sweep_keyframe_offset = 0) {
  // The sweep angle ranges from -270 to 270 over 1333ms. CSS
  // animation timing functions apply in between key frames, so we have to
  // break up the 1333ms into two keyframes (-270 to 0, then 0 to 270).
  const double elapsed_ratio =
      (elapsed_time / kArcTime) + sweep_keyframe_offset;
  const int64_t sweep_frame = base::ClampFloor<int64_t>(elapsed_ratio);
  const double arc_progress = elapsed_ratio - sweep_frame;
  // This tween is equivalent to cubic-bezier(0.4, 0.0, 0.2, 1).
  double sweep = kMaxArcSize *
                 Tween::CalculateValue(Tween::FAST_OUT_SLOW_IN, arc_progress);
  if (sweep_frame % 2 == 0) {
    sweep -= kMaxArcSize;
  }

  // This part makes sure the sweep is at least 5 degrees long. Roughly
  // equivalent to the "magic constants" in SVG's fillunfill animation.
  constexpr double kMinSweepLength = 5.0;
  if (sweep >= 0.0 && sweep < kMinSweepLength) {
    start_angle -= (kMinSweepLength - sweep);
    sweep = kMinSweepLength;
  } else if (sweep <= 0.0 && sweep > -kMinSweepLength) {
    start_angle += (-kMinSweepLength - sweep);
    sweep = -kMinSweepLength;
  }

  // To keep the sweep smooth, we have an additional rotation after each
  // arc period has elapsed. See SVG's 'rot' animation.
  const int64_t rot_keyframe = (sweep_frame / 2) % 4;
  start_angle = start_angle + rot_keyframe * kMaxArcSize;
  return ThrobberSpinningState{
      .start_angle = static_cast<SkScalar>(start_angle),
      .sweep_angle = static_cast<SkScalar>(sweep)};
}

void PaintThrobberSpinningWithState(Canvas* canvas,
                                    const Rect& bounds,
                                    SkColor color,
                                    const ThrobberSpinningState& state,
                                    std::optional<SkScalar> stroke_width) {
  PaintArc(canvas, bounds, color, state.start_angle, state.sweep_angle,
           stroke_width);
}
}  // namespace

ThrobberSpinningState CalculateThrobberSpinningState(
    base::TimeDelta elapsed_time,
    const int64_t sweep_keyframe_offset) {
  const int64_t start_angle =
      270 + base::ClampRound<int64_t>(elapsed_time / kRotationTime * 360);
  return CalculateThrobberSpinningStateWithStartAngle(elapsed_time, start_angle,
                                                      sweep_keyframe_offset);
}

void PaintThrobberSpinning(Canvas* canvas,
                           const Rect& bounds,
                           SkColor color,
                           const base::TimeDelta& elapsed_time,
                           std::optional<SkScalar> stroke_width) {
  const ThrobberSpinningState state =
      CalculateThrobberSpinningState(elapsed_time);
  PaintThrobberSpinningWithState(canvas, bounds, color, state, stroke_width);
}

void PaintThrobberSpinningWithSweepEasedIn(
    Canvas* canvas,
    const Rect& bounds,
    SkColor color,
    const base::TimeDelta& elapsed_time,
    std::optional<SkScalar> stroke_width) {
  // The second keyframe of the spinning animation is when the arc is
  // minimized, compared to the first keyframe, where it is at its maximum.
  const ThrobberSpinningState state =
      CalculateThrobberSpinningState(elapsed_time, 1);
  PaintThrobberSpinningWithState(canvas, bounds, color, state, stroke_width);
}

}  // namespace gfx

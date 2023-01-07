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
              absl::optional<SkScalar> stroke_width) {
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

void CalculateWaitingAngles(const base::TimeDelta& elapsed_time,
                            int64_t* start_angle,
                            int64_t* sweep) {
  // Calculate start and end points. The angles are counter-clockwise because
  // the throbber spins counter-clockwise. The finish angle starts at 12 o'clock
  // (90 degrees) and rotates steadily. The start angle trails 180 degrees
  // behind, except for the first half revolution, when it stays at 12 o'clock.
  constexpr auto kRevolutionTime = base::Milliseconds(1320);
  int64_t twelve_oclock = 90;
  int64_t finish_angle_cc =
      twelve_oclock +
      base::ClampRound<int64_t>(elapsed_time / kRevolutionTime * 360);
  int64_t start_angle_cc = std::max(finish_angle_cc - 180, twelve_oclock);

  // Negate the angles to convert to the clockwise numbers Skia expects.
  if (start_angle)
    *start_angle = -finish_angle_cc;
  if (sweep)
    *sweep = finish_angle_cc - start_angle_cc;
}

// This is a Skia port of the MD spinner SVG. The |start_angle| rotation
// here corresponds to the 'rotate' animation.
ThrobberSpinningState CalculateThrobberSpinningStateWithStartAngle(
    base::TimeDelta elapsed_time,
    int64_t start_angle) {
  // The sweep angle ranges from -270 to 270 over 1333ms. CSS
  // animation timing functions apply in between key frames, so we have to
  // break up the 1333ms into two keyframes (-270 to 0, then 0 to 270).
  const double elapsed_ratio = elapsed_time / kArcTime;
  const int64_t sweep_frame = base::ClampFloor<int64_t>(elapsed_ratio);
  const double arc_progress = elapsed_ratio - sweep_frame;
  // This tween is equivalent to cubic-bezier(0.4, 0.0, 0.2, 1).
  double sweep = kMaxArcSize *
                 Tween::CalculateValue(Tween::FAST_OUT_SLOW_IN, arc_progress);
  if (sweep_frame % 2 == 0)
    sweep -= kMaxArcSize;

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
                                    absl::optional<SkScalar> stroke_width) {
  PaintArc(canvas, bounds, color, state.start_angle, state.sweep_angle,
           stroke_width);
}

void PaintThrobberSpinningWithStartAngle(
    Canvas* canvas,
    const Rect& bounds,
    SkColor color,
    const base::TimeDelta& elapsed_time,
    int64_t start_angle,
    absl::optional<SkScalar> stroke_width) {
  const ThrobberSpinningState state =
      CalculateThrobberSpinningStateWithStartAngle(elapsed_time, start_angle);
  PaintThrobberSpinningWithState(canvas, bounds, color, state, stroke_width);
}

}  // namespace

ThrobberSpinningState CalculateThrobberSpinningState(
    base::TimeDelta elapsed_time) {
  const int64_t start_angle =
      270 + base::ClampRound<int64_t>(elapsed_time / kRotationTime * 360);
  return CalculateThrobberSpinningStateWithStartAngle(elapsed_time,
                                                      start_angle);
}

void PaintThrobberSpinning(Canvas* canvas,
                           const Rect& bounds,
                           SkColor color,
                           const base::TimeDelta& elapsed_time,
                           absl::optional<SkScalar> stroke_width) {
  const ThrobberSpinningState state =
      CalculateThrobberSpinningState(elapsed_time);
  PaintThrobberSpinningWithState(canvas, bounds, color, state, stroke_width);
}

void PaintThrobberWaiting(Canvas* canvas,
                          const Rect& bounds,
                          SkColor color,
                          const base::TimeDelta& elapsed_time,
                          absl::optional<SkScalar> stroke_width) {
  int64_t start_angle = 0, sweep = 0;
  CalculateWaitingAngles(elapsed_time, &start_angle, &sweep);
  PaintArc(canvas, bounds, color, start_angle, sweep, stroke_width);
}

void PaintThrobberSpinningAfterWaiting(Canvas* canvas,
                                       const Rect& bounds,
                                       SkColor color,
                                       const base::TimeDelta& elapsed_time,
                                       ThrobberWaitingState* waiting_state,
                                       absl::optional<SkScalar> stroke_width) {
  int64_t waiting_start_angle = 0, waiting_sweep = 0;
  CalculateWaitingAngles(waiting_state->elapsed_time, &waiting_start_angle,
                         &waiting_sweep);

  // |arc_time_offset| is the effective amount of time one would have to wait
  // for the "spinning" sweep to match |waiting_sweep|. Brute force calculation.
  if (waiting_state->arc_time_offset.is_zero()) {
    for (int64_t arc_ms = 0; arc_ms <= kArcTime.InMillisecondsRoundedUp();
         ++arc_ms) {
      const base::TimeDelta arc_time =
          std::min(base::Milliseconds(arc_ms), kArcTime);
      if (kMaxArcSize * Tween::CalculateValue(Tween::FAST_OUT_SLOW_IN,
                                              arc_time / kArcTime) >=
          waiting_sweep) {
        // Add kArcTime to sidestep the |sweep_keyframe == 0| offset below.
        waiting_state->arc_time_offset = kArcTime + arc_time;
        break;
      }
    }
  }

  // Blend the color between "waiting" and "spinning" states.
  constexpr auto kColorFadeTime = base::Milliseconds(900);
  const float color_progress = static_cast<float>(Tween::CalculateValue(
      Tween::LINEAR_OUT_SLOW_IN, std::min(elapsed_time / kColorFadeTime, 1.0)));
  const SkColor blend_color =
      color_utils::AlphaBlend(color, waiting_state->color, color_progress);

  const int64_t start_angle =
      waiting_start_angle +
      base::ClampRound<int64_t>(elapsed_time / kRotationTime * 360);
  const base::TimeDelta effective_elapsed_time =
      elapsed_time + waiting_state->arc_time_offset;

  PaintThrobberSpinningWithStartAngle(canvas, bounds, blend_color,
                                      effective_elapsed_time, start_angle,
                                      stroke_width);
}

GFX_EXPORT void PaintNewThrobberWaiting(Canvas* canvas,
                                        const RectF& throbber_container_bounds,
                                        SkColor color,
                                        const base::TimeDelta& elapsed_time) {
  // Cycle time for the waiting throbber.
  constexpr auto kNewThrobberWaitingCycleTime = base::Seconds(1);

  // The throbber bounces back and forth. We map the elapsed time to 0->2. Time
  // 0->1 represents when the throbber moves left to right, time 1->2 represents
  // right to left.
  float time = 2.0f * (elapsed_time % kNewThrobberWaitingCycleTime) /
               kNewThrobberWaitingCycleTime;
  // 1 -> 2 values mirror back to 1 -> 0 values to represent right-to-left.
  const bool going_back = time > 1.0f;
  if (going_back)
    time = 2.0f - time;
  // This animation should be fast in the middle and slow at the edges.
  time = Tween::CalculateValue(Tween::EASE_IN_OUT, time);
  const float min_width = throbber_container_bounds.height();
  // The throbber animation stretches longer when moving in (left to right) than
  // when going back.
  const float throbber_width =
      (going_back ? 0.75f : 1.0f) * throbber_container_bounds.width();

  // These bounds keep at least |min_width| of the throbber visible (inside the
  // throbber bounds).
  const float min_x =
      throbber_container_bounds.x() - throbber_width + min_width;
  const float max_x = throbber_container_bounds.right() - min_width;

  RectF bounds = throbber_container_bounds;
  // Linear interpolation between |min_x| and |max_x|.
  bounds.set_x(time * (max_x - min_x) + min_x);
  bounds.set_width(throbber_width);
  // The throbber is designed to go out of bounds, but it should not be rendered
  // outside |throbber_container_bounds|. This clips the throbber to the edges,
  // which gives a smooth bouncing effect.
  bounds.Intersect(throbber_container_bounds);

  cc::PaintFlags flags;
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  // Draw with circular end caps.
  canvas->DrawRoundRect(bounds, bounds.height() / 2, flags);
}

}  // namespace gfx

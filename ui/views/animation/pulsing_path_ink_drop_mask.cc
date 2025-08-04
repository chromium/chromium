// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/pulsing_path_ink_drop_mask.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/view_class_properties.h"

namespace {

// Cycle duration of ink drop pulsing animation used for in-product help.
constexpr base::TimeDelta kFeaturePromoPulseDuration = base::Milliseconds(800);

// Max inset for pulsing animation.
constexpr double kFeaturePromoPulseMaxInsetDip = 3.0;

// Returns how much the pulsing inkdrop should pulse inwards from the
// container's `bounds`, in DIP.
double GetThrobInsetDip(const SkRect& bounds) {
  // Verify that neither axis of `bounds` is zero.
  if (bounds.isEmpty()) {
    return kFeaturePromoPulseMaxInsetDip;
  }
  // The pulsing inkdrop will pulse between the full view bounds and the view
  // bounds modified by some inset. Since this involves scaling, if the shorter
  // axis A is scaled by X, then the longer axis B pulses by X * (B / A). This
  // excessive pulsing looks bad.
  //
  // To correct for this, adjust so that the longer axis pulses by
  // kFeaturePromoPulseMaxInsetDip, and the shorter axis pulses by
  // proportionately less.
  double ratio = bounds.width() / bounds.height();
  ratio = std::min(ratio, 1.0 / ratio);
  return ratio * kFeaturePromoPulseMaxInsetDip;
}

}  // namespace

namespace views {
PulsingPathInkDropMask::PulsingPathInkDropMask(views::View* layer_container,
                                               SkPath path)
    : AnimationDelegateViews(layer_container),
      views::InkDropMask(layer_container->size()),
      layer_container_(layer_container),
      path_(path),
      initial_rect_(path.getBounds()),
      throb_inset_(GetThrobInsetDip(initial_rect_)),
      throb_animation_(this) {
  throb_animation_.SetThrobDuration(kFeaturePromoPulseDuration);
  throb_animation_.StartThrobbing(-1);
}

PulsingPathInkDropMask::~PulsingPathInkDropMask() = default;

void PulsingPathInkDropMask::OnPaintLayer(const ui::PaintContext& context) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  ui::PaintRecorder recorder(context, layer()->size());

  gfx::RectF bounds(layer()->bounds());

  const gfx::Insets* const internal_padding =
      layer_container_->GetProperty(views::kInternalPaddingKey);
  if (internal_padding) {
    gfx::Insets insets(*internal_padding);
    if (layer_container_->GetFlipCanvasOnPaintForRTLUI() &&
        base::i18n::IsRTL()) {
      insets.set_left(internal_padding->right());
      insets.set_right(internal_padding->left());
    }
    bounds.Inset(gfx::InsetsF(insets));
  }

  const float current_inset =
      throb_animation_.CurrentValueBetween(0.0f, throb_inset_);
  bounds.Inset(gfx::InsetsF(current_inset));

  // Transform the highlight path to the target region.
  //
  // Note: this is not the correct calculation if the original inkdrop isn't
  // centered in the target space. However, it should work for the vast majority
  // of cases, and is no worse than what PulsingInkDropMask does.
  SkPath path = path_.makeTransform(
      SkMatrix::MakeRectToRect(initial_rect_, gfx::RectFToSkRect(bounds),
                               SkMatrix::ScaleToFit::kCenter_ScaleToFit));
  recorder.canvas()->DrawPath(path, flags);
}

void PulsingPathInkDropMask::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, &throb_animation_);
  layer()->SchedulePaint(gfx::Rect(layer()->size()));

  // This is a workaround for crbug.com/935808: for scale factors >1,
  // invalidating the mask layer doesn't cause the whole layer to be repainted
  // on screen. TODO(crbug.com/40615539): remove this workaround once the bug is
  // fixed.
  layer_container_->SchedulePaint();
}
}  // namespace views

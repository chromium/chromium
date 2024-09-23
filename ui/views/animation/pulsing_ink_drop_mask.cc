// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/pulsing_ink_drop_mask.h"

#include "base/i18n/rtl.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/views/view_class_properties.h"

namespace {
// Cycle duration of ink drop pulsing animation used for in-product help.
constexpr base::TimeDelta kFeaturePromoPulseDuration = base::Milliseconds(800);

// Max inset for pulsing animation.
constexpr float kFeaturePromoPulseInsetDip = 3.0f;
}  // namespace

namespace views {
PulsingInkDropMask::PulsingInkDropMask(views::View* layer_container)
    : AnimationDelegateViews(layer_container),
      views::InkDropMask(layer_container->size()),
      layer_container_(layer_container),
      normal_corner_radius_(layer_container->height() / 2.0f),
      throb_animation_(this) {
  throb_animation_.SetThrobDuration(kFeaturePromoPulseDuration);
  throb_animation_.StartThrobbing(-1);
}

void PulsingInkDropMask::OnPaintLayer(const ui::PaintContext& context) {
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
      throb_animation_.CurrentValueBetween(0.0f, kFeaturePromoPulseInsetDip);
  bounds.Inset(gfx::InsetsF(current_inset));
  const float corner_radius = normal_corner_radius_ - current_inset;

  recorder.canvas()->DrawRoundRect(bounds, corner_radius, flags);
}

void PulsingInkDropMask::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, &throb_animation_);
  layer()->SchedulePaint(gfx::Rect(layer()->size()));

  // This is a workaround for crbug.com/935808: for scale factors >1,
  // invalidating the mask layer doesn't cause the whole layer to be repainted
  // on screen. TODO(crbug.com/40615539): remove this workaround once the bug is
  // fixed.
  layer_container_->SchedulePaint();
}
}  // namespace views

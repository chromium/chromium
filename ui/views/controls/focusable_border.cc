// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focusable_border.h"

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/textfield/textfield.h"

namespace {

constexpr int kInsetSize = 1;

}  // namespace

namespace views {

FocusableBorder::FocusableBorder()
    : insets_(kInsetSize), corner_radius_(FocusRing::kDefaultCornerRadiusDp) {}

FocusableBorder::~FocusableBorder() = default;

void FocusableBorder::SetColorId(const std::optional<ui::ColorId>& color_id) {
  override_color_id_ = color_id;
}

void FocusableBorder::Paint(const View& view, gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setColor(GetCurrentColor(view));

  gfx::ScopedCanvas scoped(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();

  const float kStrokeWidth = dsf;
  flags.setStrokeWidth(kStrokeWidth);

  // Scale the rect and snap to pixel boundaries.
  gfx::RectF rect(gfx::ScaleToEnclosedRect(view.GetLocalBounds(), dsf));
  rect.Inset(gfx::InsetsF(kStrokeWidth / 2.0f));

  SkPath path;
  flags.setAntiAlias(true);
  float corner_radius_px = corner_radius_ * dsf;
  path.addRoundRect(gfx::RectFToSkRect(rect), corner_radius_px,
                    corner_radius_px);

  canvas->DrawPath(path, flags);
}

gfx::Insets FocusableBorder::GetInsets() const {
  return insets_;
}

gfx::Size FocusableBorder::GetMinimumSize() const {
  return gfx::Size();
}

void FocusableBorder::SetInsets(const gfx::Insets& insets) {
  insets_ = insets;
}

void FocusableBorder::SetCornerRadius(float radius) {
  corner_radius_ = radius;
}

SkColor FocusableBorder::GetCurrentColor(const View& view) const {
  ui::ColorId color_id = ui::kColorFocusableBorderUnfocused;
  if (override_color_id_)
    color_id = *override_color_id_;

  SkColor color = view.GetColorProvider()->GetColor(color_id);
  return view.GetEnabled() ? color
                           : color_utils::BlendTowardMaxContrast(
                                 color, gfx::kDisabledControlAlpha);
}

}  // namespace views

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focusable_border.h"

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/textfield/textfield.h"

namespace {

constexpr int kInsetSize = 1;

}  // namespace

namespace views {

FocusableBorder::FocusableBorder() : insets_(kInsetSize) {}

FocusableBorder::~FocusableBorder() = default;

void FocusableBorder::SetColorId(
    const base::Optional<ui::NativeTheme::ColorId>& color_id) {
  override_color_id_ = color_id;
}

void FocusableBorder::Paint(const View& view, gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setColor(GetCurrentColor(view));

  gfx::ScopedCanvas scoped(canvas);
  float dsf = canvas->UndoDeviceScaleFactor();

  constexpr int kStrokeWidthPx = 1;
  flags.setStrokeWidth(SkIntToScalar(kStrokeWidthPx));

  // Scale the rect and snap to pixel boundaries.
  gfx::RectF rect(gfx::ScaleToEnclosedRect(view.GetLocalBounds(), dsf));
  rect.Inset(gfx::InsetsF(kStrokeWidthPx / 2.0f));

  SkPath path;
    flags.setAntiAlias(true);
    float corner_radius_px = kCornerRadiusDp * dsf;
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

void FocusableBorder::SetInsets(int top, int left, int bottom, int right) {
  insets_.Set(top, left, bottom, right);
}

void FocusableBorder::SetInsets(int vertical, int horizontal) {
  SetInsets(vertical, horizontal, vertical, horizontal);
}

SkColor FocusableBorder::GetCurrentColor(const View& view) const {
  ui::NativeTheme::ColorId color_id =
      ui::NativeTheme::kColorId_UnfocusedBorderColor;
  if (override_color_id_)
    color_id = *override_color_id_;

  SkColor color = view.GetNativeTheme()->GetSystemColor(color_id);
  return view.GetEnabled() ? color
                           : color_utils::BlendTowardMaxContrast(
                                 color, gfx::kDisabledControlAlpha);
}

}  // namespace views

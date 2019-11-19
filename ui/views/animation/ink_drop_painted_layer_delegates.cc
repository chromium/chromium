// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_painted_layer_delegates.h"

#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/skia_util.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// BasePaintedLayerDelegate
//

BasePaintedLayerDelegate::BasePaintedLayerDelegate(SkColor color)
    : color_(color) {}

BasePaintedLayerDelegate::~BasePaintedLayerDelegate() = default;

gfx::Vector2dF BasePaintedLayerDelegate::GetCenteringOffset() const {
  return gfx::RectF(GetPaintedBounds()).CenterPoint().OffsetFromOrigin();
}

void BasePaintedLayerDelegate::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

////////////////////////////////////////////////////////////////////////////////
//
// CircleLayerDelegate
//

CircleLayerDelegate::CircleLayerDelegate(SkColor color, int radius)
    : BasePaintedLayerDelegate(color), radius_(radius) {}

CircleLayerDelegate::~CircleLayerDelegate() = default;

gfx::RectF CircleLayerDelegate::GetPaintedBounds() const {
  const int diameter = radius_ * 2;
  return gfx::RectF(0, 0, diameter, diameter);
}

void CircleLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  cc::PaintFlags flags;
  flags.setColor(color());
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  ui::PaintRecorder recorder(context,
                             gfx::ToEnclosingRect(GetPaintedBounds()).size());
  gfx::Canvas* canvas = recorder.canvas();

  canvas->DrawCircle(GetPaintedBounds().CenterPoint(), radius_, flags);
}

////////////////////////////////////////////////////////////////////////////////
//
// RectangleLayerDelegate
//

RectangleLayerDelegate::RectangleLayerDelegate(SkColor color, gfx::SizeF size)
    : BasePaintedLayerDelegate(color), size_(size) {}

RectangleLayerDelegate::~RectangleLayerDelegate() = default;

gfx::RectF RectangleLayerDelegate::GetPaintedBounds() const {
  return gfx::RectF(size_);
}

void RectangleLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  cc::PaintFlags flags;
  flags.setColor(color());
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  ui::PaintRecorder recorder(context, gfx::ToCeiledSize(size_));
  gfx::Canvas* canvas = recorder.canvas();
  canvas->DrawRect(GetPaintedBounds(), flags);
}

////////////////////////////////////////////////////////////////////////////////
//
// RoundedRectangleLayerDelegate
//

RoundedRectangleLayerDelegate::RoundedRectangleLayerDelegate(
    SkColor color,
    const gfx::SizeF& size,
    int corner_radius)
    : BasePaintedLayerDelegate(color),
      size_(size),
      corner_radius_(corner_radius) {}

RoundedRectangleLayerDelegate::~RoundedRectangleLayerDelegate() = default;

gfx::RectF RoundedRectangleLayerDelegate::GetPaintedBounds() const {
  return gfx::RectF(size_);
}

void RoundedRectangleLayerDelegate::OnPaintLayer(
    const ui::PaintContext& context) {
  cc::PaintFlags flags;
  flags.setColor(color());
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  ui::PaintRecorder recorder(context, gfx::ToCeiledSize(size_));
  const float dsf = recorder.canvas()->UndoDeviceScaleFactor();
  gfx::RectF rect = GetPaintedBounds();
  rect.Scale(dsf);
  recorder.canvas()->DrawRoundRect(gfx::ToEnclosingRect(rect),
                                   dsf * corner_radius_, flags);
}

////////////////////////////////////////////////////////////////////////////////
//
// BorderShadowLayerDelegate
//

BorderShadowLayerDelegate::BorderShadowLayerDelegate(
    const std::vector<gfx::ShadowValue>& shadows,
    const gfx::Rect& shadowed_area_bounds,
    SkColor fill_color,
    int corner_radius)
    : BasePaintedLayerDelegate(gfx::kPlaceholderColor),
      shadows_(shadows),
      bounds_(shadowed_area_bounds),
      fill_color_(fill_color),
      corner_radius_(corner_radius) {}

BorderShadowLayerDelegate::~BorderShadowLayerDelegate() = default;

gfx::RectF BorderShadowLayerDelegate::GetPaintedBounds() const {
  gfx::Rect total_rect(bounds_);
  total_rect.Inset(gfx::ShadowValue::GetMargin(shadows_));
  return gfx::RectF(total_rect);
}

gfx::Vector2dF BorderShadowLayerDelegate::GetCenteringOffset() const {
  return gfx::RectF(bounds_).CenterPoint().OffsetFromOrigin();
}

void BorderShadowLayerDelegate::OnPaintLayer(const ui::PaintContext& context) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(fill_color_);

  gfx::RectF rrect_bounds =
      gfx::RectF(bounds_) - GetPaintedBounds().OffsetFromOrigin();
  SkRRect r_rect = SkRRect::MakeRectXY(gfx::RectFToSkRect(rrect_bounds),
                                       corner_radius_, corner_radius_);

  // First the fill color.
  ui::PaintRecorder recorder(context,
                             gfx::ToCeiledSize(GetPaintedBounds().size()));
  recorder.canvas()->sk_canvas()->drawRRect(r_rect, flags);

  // Now the shadow.
  flags.setLooper(gfx::CreateShadowDrawLooper(shadows_));
  recorder.canvas()->sk_canvas()->clipRRect(r_rect, SkClipOp::kDifference,
                                            true);
  recorder.canvas()->sk_canvas()->drawRRect(r_rect, flags);
}

}  // namespace views

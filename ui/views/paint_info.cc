// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/paint_info.h"
#include "base/feature_list.h"
#include "ui/views/views_features.h"

namespace views {
namespace {

gfx::Rect GetSnappedRecordingBoundsInternal(
    const gfx::Rect& paint_recording_bounds,
    float device_scale_factor,
    const gfx::Size& parent_size,
    const gfx::Rect& child_bounds) {
  const gfx::Vector2d& child_origin = child_bounds.OffsetFromOrigin();

  int right = child_origin.x() + child_bounds.width();
  int bottom = child_origin.y() + child_bounds.height();

  int new_x = std::round(child_origin.x() * device_scale_factor);
  int new_y = std::round(child_origin.y() * device_scale_factor);

  int new_right;
  int new_bottom;

  bool empty = paint_recording_bounds.IsEmpty();

  if (right == parent_size.width() && !empty)
    new_right = paint_recording_bounds.width();
  else
    new_right = std::round(right * device_scale_factor);

  if (bottom == parent_size.height() && !empty)
    new_bottom = paint_recording_bounds.height();
  else
    new_bottom = std::round(bottom * device_scale_factor);

  return gfx::Rect(new_x + paint_recording_bounds.x(),
                   new_y + paint_recording_bounds.y(), new_right - new_x,
                   new_bottom - new_y);
}

// Layer's paint info should use the corner scaling logic to compute
// the recording which is what Views uses to compte the view's
// paint_recording_bounds_, with exception that a view touches the right/bottom
// edges of the parent, and its layer has to be able to paint to these
// edges. Such cases should be handled case by case basis.
gfx::Rect GetViewsLayerRecordingBounds(const ui::PaintContext& context,
                                       const gfx::Rect& child_bounds) {
  if (!context.is_pixel_canvas())
    return gfx::Rect(child_bounds.size());
  return gfx::Rect(GetSnappedRecordingBoundsInternal(
                       gfx::Rect(), context.device_scale_factor(),
                       gfx::Size() /* not used */, child_bounds)
                       .size());
}

}  // namespace

// static
PaintInfo PaintInfo::CreateRootPaintInfo(const ui::PaintContext& root_context,
                                         const gfx::Size& size) {
  return PaintInfo(root_context, size);
}

//  static
PaintInfo PaintInfo::CreateChildPaintInfo(const PaintInfo& parent_paint_info,
                                          const gfx::Rect& bounds,
                                          const gfx::Size& parent_size,
                                          ScaleType scale_type,
                                          bool is_layer,
                                          bool needs_paint) {
  return PaintInfo(parent_paint_info, bounds, parent_size, scale_type, is_layer,
                   needs_paint);
}

PaintInfo::~PaintInfo() = default;

bool PaintInfo::IsPixelCanvas() const {
  return context().is_pixel_canvas();
}

bool PaintInfo::ShouldPaint() const {
  if (base::FeatureList::IsEnabled(features::kEnableViewPaintOptimization))
    return needs_paint_;

  return context().IsRectInvalid(gfx::Rect(paint_recording_size()));
}

PaintInfo::PaintInfo(const PaintInfo& other)
    : paint_recording_scale_x_(other.paint_recording_scale_x_),
      paint_recording_scale_y_(other.paint_recording_scale_y_),
      paint_recording_bounds_(other.paint_recording_bounds_),
      offset_from_parent_(other.offset_from_parent_),
      context_(other.context(), gfx::Vector2d()),
      root_context_(nullptr),
      needs_paint_(false) {}

// The root layer should use the ScaleToEnclosingRect, the same logic that
// cc(chrome compositor) is using.
PaintInfo::PaintInfo(const ui::PaintContext& root_context,
                     const gfx::Size& size)
    : paint_recording_scale_x_(root_context.is_pixel_canvas()
                                   ? root_context.device_scale_factor()
                                   : 1.f),
      paint_recording_scale_y_(paint_recording_scale_x_),
      paint_recording_bounds_(
          gfx::ScaleToEnclosingRect(gfx::Rect(size), paint_recording_scale_x_)),
      context_(root_context, gfx::Vector2d()),
      root_context_(&root_context) {}

PaintInfo::PaintInfo(const PaintInfo& parent_paint_info,
                     const gfx::Rect& bounds,
                     const gfx::Size& parent_size,
                     ScaleType scale_type,
                     bool is_layer,
                     bool needs_paint)
    : paint_recording_scale_x_(1.f),
      paint_recording_scale_y_(1.f),
      paint_recording_bounds_(
          is_layer ? GetViewsLayerRecordingBounds(parent_paint_info.context(),
                                                  bounds)
                   : parent_paint_info.GetSnappedRecordingBounds(parent_size,
                                                                 bounds)),
      offset_from_parent_(
          paint_recording_bounds_.OffsetFromOrigin() -
          parent_paint_info.paint_recording_bounds_.OffsetFromOrigin()),
      context_(parent_paint_info.context(), offset_from_parent_),
      root_context_(nullptr),
      needs_paint_(needs_paint) {
  if (IsPixelCanvas()) {
    if (scale_type == ScaleType::kUniformScaling) {
      paint_recording_scale_x_ = paint_recording_scale_y_ =
          context().device_scale_factor();
    } else if (scale_type == ScaleType::kScaleWithEdgeSnapping) {
      if (bounds.size().width() > 0) {
        paint_recording_scale_x_ =
            static_cast<float>(paint_recording_bounds_.width()) /
            static_cast<float>(bounds.size().width());
      }
      if (bounds.size().height() > 0) {
        paint_recording_scale_y_ =
            static_cast<float>(paint_recording_bounds_.height()) /
            static_cast<float>(bounds.size().height());
      }
    }
  }
}

gfx::Rect PaintInfo::GetSnappedRecordingBounds(
    const gfx::Size& parent_size,
    const gfx::Rect& child_bounds) const {
  if (!IsPixelCanvas())
    return (child_bounds + paint_recording_bounds_.OffsetFromOrigin());

  return GetSnappedRecordingBoundsInternal(paint_recording_bounds_,
                                           context().device_scale_factor(),
                                           parent_size, child_bounds);
}

}  // namespace views

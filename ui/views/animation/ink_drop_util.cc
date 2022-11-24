// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_util.h"

#include <cmath>

#include "base/check_op.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/view.h"
#include "ui/views/views_features.h"

namespace views {

gfx::Transform GetTransformSubpixelCorrection(const gfx::Transform& transform,
                                              float device_scale_factor) {
  gfx::PointF origin = transform.MapPoint(gfx::PointF());
  const gfx::Vector2dF offset_in_dip = origin.OffsetFromOrigin();

  // Scale the origin to screen space
  origin.Scale(device_scale_factor);

  // Compute the rounded offset in screen space and finally unscale it back to
  // DIP space.
  gfx::Vector2dF aligned_offset_in_dip = origin.OffsetFromOrigin();
  aligned_offset_in_dip.set_x(std::round(aligned_offset_in_dip.x()));
  aligned_offset_in_dip.set_y(std::round(aligned_offset_in_dip.y()));
  aligned_offset_in_dip.InvScale(device_scale_factor);

  // Compute the subpixel offset correction and apply it to the transform.
  gfx::Transform subpixel_correction;
  subpixel_correction.Translate(aligned_offset_in_dip - offset_in_dip);
#if DCHECK_IS_ON()
  const float kEpsilon = 0.0001f;

  gfx::Transform transform_corrected(transform);
  transform_corrected.PostConcat(subpixel_correction);
  gfx::Point3F offset = transform_corrected.MapPoint(gfx::Point3F());
  offset.Scale(device_scale_factor);

  if (!std::isnan(offset.x()))
    DCHECK_LT(std::abs(std::round(offset.x()) - offset.x()), kEpsilon);
  if (!std::isnan(offset.y()))
    DCHECK_LT(std::abs(std::round(offset.y()) - offset.y()), kEpsilon);
#endif
  return subpixel_correction;
}

bool UsingPlatformHighContrastInkDrop(const View* view) {
  return view->GetWidget() &&
         view->GetNativeTheme()->GetDefaultSystemColorScheme() ==
             ui::NativeTheme::ColorScheme::kPlatformHighContrast &&
         base::FeatureList::IsEnabled(
             features::kEnablePlatformHighContrastInkDrop);
}

}  // namespace views

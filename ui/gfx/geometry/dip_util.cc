// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/dip_util.h"

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace gfx {

PointF ConvertPointToDips(const Point& point_in_pixels,
                          float device_scale_factor) {
  return ScalePoint(PointF(point_in_pixels), 1.f / device_scale_factor);
}

PointF ConvertPointToDips(const PointF& point_in_pixels,
                          float device_scale_factor) {
  return ScalePoint(point_in_pixels, 1.f / device_scale_factor);
}

PointF ConvertPointToPixels(const Point& point_in_dips,
                            float device_scale_factor) {
  return ScalePoint(PointF(point_in_dips), device_scale_factor);
}

PointF ConvertPointToPixels(const PointF& point_in_dips,
                            float device_scale_factor) {
  return ScalePoint(point_in_dips, device_scale_factor);
}

SizeF ConvertSizeToDips(const Size& size_in_pixels, float device_scale_factor) {
  return ScaleSize(SizeF(size_in_pixels), 1.f / device_scale_factor);
}

SizeF ConvertSizeToDips(const SizeF& size_in_pixels,
                        float device_scale_factor) {
  return ScaleSize(size_in_pixels, 1.f / device_scale_factor);
}

Insets ConvertInsetsToDIP(float scale_factor,
                          const gfx::Insets& insets_in_pixel) {
  if (scale_factor == 1.f)
    return insets_in_pixel;
  return insets_in_pixel.Scale(1.f / scale_factor);
}

Rect ConvertRectToDIP(float scale_factor, const Rect& rect_in_pixel) {
  if (scale_factor == 1.f)
    return rect_in_pixel;
  return ToFlooredRectDeprecated(
      ScaleRect(RectF(rect_in_pixel), 1.f / scale_factor));
}

Insets ConvertInsetsToPixel(float scale_factor,
                            const gfx::Insets& insets_in_dip) {
  if (scale_factor == 1.f)
    return insets_in_dip;
  return insets_in_dip.Scale(scale_factor);
}

Size ConvertSizeToPixel(float scale_factor, const Size& size_in_dip) {
  if (scale_factor == 1.f)
    return size_in_dip;
  return ScaleToFlooredSize(size_in_dip, scale_factor);
}

Rect ConvertRectToPixel(float scale_factor, const Rect& rect_in_dip) {
  // Use ToEnclosingRect() to ensure we paint all the possible pixels
  // touched. ToEnclosingRect() floors the origin, and ceils the max
  // coordinate. To do otherwise (such as flooring the size) potentially
  // results in rounding down and not drawing all the pixels that are
  // touched.
  if (scale_factor == 1.f)
    return rect_in_dip;
  return ToEnclosingRect(
      RectF(ScalePoint(gfx::PointF(rect_in_dip.origin()), scale_factor),
            ScaleSize(gfx::SizeF(rect_in_dip.size()), scale_factor)));
}

}  // namespace gfx

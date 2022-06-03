// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/dip_util.h"

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace gfx {

#if defined(OS_MAC)
// Returns true if the floating point value is holding an integer, modulo
// floating point error. The value `f` can be safely converted to its integer
// form with base::ClampRound().
static bool IsIntegerInFloat(float f) {
  return std::abs(f - base::ClampRound(f)) < 0.01f;
}
#endif

PointF ConvertPointToDips(const Point& point_in_pixels,
                          float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScalePoint(PointF(point_in_pixels), 1.f / device_scale_factor);
}

PointF ConvertPointToDips(const PointF& point_in_pixels,
                          float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScalePoint(point_in_pixels, 1.f / device_scale_factor);
}

PointF ConvertPointToPixels(const Point& point_in_dips,
                            float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScalePoint(PointF(point_in_dips), device_scale_factor);
}

PointF ConvertPointToPixels(const PointF& point_in_dips,
                            float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScalePoint(point_in_dips, device_scale_factor);
}

SizeF ConvertSizeToDips(const Size& size_in_pixels, float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleSize(SizeF(size_in_pixels), 1.f / device_scale_factor);
}

SizeF ConvertSizeToDips(const SizeF& size_in_pixels,
                        float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleSize(size_in_pixels, 1.f / device_scale_factor);
}

SizeF ConvertSizeToPixels(const Size& size_in_dips, float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleSize(SizeF(size_in_dips), device_scale_factor);
}

SizeF ConvertSizeToPixels(const SizeF& size_in_dips,
                          float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleSize(size_in_dips, device_scale_factor);
}

RectF ConvertRectToDips(const Rect& rect_in_pixels, float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleRect(RectF(rect_in_pixels), 1.f / device_scale_factor);
}

RectF ConvertRectToDips(const RectF& rect_in_pixels,
                        float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleRect(rect_in_pixels, 1.f / device_scale_factor);
}

RectF ConvertRectToPixels(const Rect& rect_in_dips, float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleRect(RectF(rect_in_dips), device_scale_factor);
}

RectF ConvertRectToPixels(const RectF& rect_in_dips,
                          float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleRect(rect_in_dips, device_scale_factor);
}

InsetsF ConvertInsetsToDips(const gfx::Insets& insets_in_pixels,
                            float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleInsets(InsetsF(insets_in_pixels), 1.f / device_scale_factor);
}

InsetsF ConvertInsetsToDips(const gfx::InsetsF& insets_in_pixels,
                            float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleInsets(insets_in_pixels, 1.f / device_scale_factor);
}

InsetsF ConvertInsetsToPixels(const gfx::Insets& insets_in_dips,
                              float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleInsets(InsetsF(insets_in_dips), device_scale_factor);
}

InsetsF ConvertInsetsToPixels(const gfx::InsetsF& insets_in_dips,
                              float device_scale_factor) {
#if defined(OS_MAC)
  // Device scale factor on MacOSX is always an integer.
  DCHECK(IsIntegerInFloat(device_scale_factor));
#endif
  return ScaleInsets(insets_in_dips, device_scale_factor);
}

}  // namespace gfx

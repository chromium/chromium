// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_DIP_UTIL_H_
#define UI_GFX_GEOMETRY_DIP_UTIL_H_

#include "ui/gfx/geometry/geometry_export.h"

namespace gfx {

class Insets;
class InsetsF;
class Point;
class PointF;
class Rect;
class RectF;
class Size;
class SizeF;

// This file contains helper functions to move between DIPs (device-independent
// pixels) and physical pixels, by multiplying or dividing by device scale
// factor. These help show the intent of the caller by naming the operation,
// instead of directly performing a scale operation. More complicated
// transformations between coordinate spaces than DIP<->physical pixels should
// be done via more explicit means.
//
// Note that functions that receive integer values will convert them to floating
// point values, which can itself be a lossy operation for large integers. The
// intention of these methods is to be used for UI values which are relatively
// small.

GEOMETRY_EXPORT gfx::PointF ConvertPointToDips(
    const gfx::Point& point_in_pixels,
    float device_scale_factor);
GEOMETRY_EXPORT gfx::PointF ConvertPointToDips(
    const gfx::PointF& point_in_pixels,
    float device_scale_factor);

GEOMETRY_EXPORT gfx::PointF ConvertPointToPixels(
    const gfx::Point& point_in_dips,
    float device_scale_factor);
GEOMETRY_EXPORT gfx::PointF ConvertPointToPixels(
    const gfx::PointF& point_in_dips,
    float device_scale_factor);

GEOMETRY_EXPORT gfx::SizeF ConvertSizeToDips(const gfx::Size& size_in_pixels,
                                             float device_scale_factor);
GEOMETRY_EXPORT gfx::SizeF ConvertSizeToDips(const gfx::SizeF& size_in_pixels,
                                             float device_scale_factor);

GEOMETRY_EXPORT gfx::SizeF ConvertSizeToPixels(const gfx::Size& size_in_dips,
                                               float device_scale_factor);
GEOMETRY_EXPORT gfx::SizeF ConvertSizeToPixels(const gfx::SizeF& size_in_dips,
                                               float device_scale_factor);

GEOMETRY_EXPORT gfx::RectF ConvertRectToDips(const gfx::Rect& rect_in_pixels,
                                             float device_scale_factor);
GEOMETRY_EXPORT gfx::RectF ConvertRectToDips(const gfx::RectF& rect_in_pixels,
                                             float device_scale_factor);

GEOMETRY_EXPORT gfx::RectF ConvertRectToPixels(const gfx::Rect& rect_in_dips,
                                               float device_scale_factor);
GEOMETRY_EXPORT gfx::RectF ConvertRectToPixels(const gfx::RectF& rect_in_dips,
                                               float device_scale_factor);

GEOMETRY_EXPORT gfx::InsetsF ConvertInsetsToDips(
    const gfx::Insets& insets_in_pixels,
    float device_scale_factor);
GEOMETRY_EXPORT gfx::InsetsF ConvertInsetsToDips(
    const gfx::InsetsF& insets_in_pixels,
    float device_scale_factor);

GEOMETRY_EXPORT gfx::InsetsF ConvertInsetsToPixels(
    const gfx::Insets& insets_in_dips,
    float device_scale_factor);
GEOMETRY_EXPORT gfx::InsetsF ConvertInsetsToPixels(
    const gfx::InsetsF& insets_in_dips,
    float device_scale_factor);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_DIP_UTIL_H_

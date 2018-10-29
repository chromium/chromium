// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_DIP_UTIL_H_
#define UI_GFX_GEOMETRY_DIP_UTIL_H_

#include "ui/gfx/geometry/geometry_export.h"

namespace gfx {

class Insets;
class Point;
class PointF;
class Rect;
class Size;

GEOMETRY_EXPORT gfx::Insets ConvertInsetsToDIP(
    float scale_factor,
    const gfx::Insets& insets_in_pixel);
GEOMETRY_EXPORT gfx::Point ConvertPointToDIP(float scale_factor,
                                             const gfx::Point& point_in_pixel);
GEOMETRY_EXPORT gfx::PointF ConvertPointToDIP(
    float scale_factor,
    const gfx::PointF& point_in_pixel);
GEOMETRY_EXPORT gfx::Size ConvertSizeToDIP(float scale_factor,
                                           const gfx::Size& size_in_pixel);
GEOMETRY_EXPORT gfx::Rect ConvertRectToDIP(float scale_factor,
                                           const gfx::Rect& rect_in_pixel);

GEOMETRY_EXPORT gfx::Insets ConvertInsetsToPixel(
    float scale_factor,
    const gfx::Insets& insets_in_dip);
GEOMETRY_EXPORT gfx::Point ConvertPointToPixel(float scale_factor,
                                               const gfx::Point& point_in_dip);
GEOMETRY_EXPORT gfx::PointF ConvertPointToPixel(
    float scale_factor,
    const gfx::PointF& point_in_dip);
GEOMETRY_EXPORT gfx::Size ConvertSizeToPixel(float scale_factor,
                                             const gfx::Size& size_in_dip);
GEOMETRY_EXPORT gfx::Rect ConvertRectToPixel(float scale_factor,
                                             const gfx::Rect& rect_in_dip);
}  // gfx

#endif  // UI_GFX_GEOMETRY_DIP_UTIL_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_DIP_UTIL_H_
#define UI_COMPOSITOR_DIP_UTIL_H_

#include "ui/compositor/compositor_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace gfx {
class Point;
class Size;
class Rect;
}  // namespace gfx

namespace ui {
class Layer;

COMPOSITOR_EXPORT float GetDeviceScaleFactor(const Layer* layer);

// Utility functions that convert point/size/rect between
// DIP and pixel coordinates system.
COMPOSITOR_EXPORT gfx::Point ConvertPointToDIP(
    const Layer* layer,
    const gfx::Point& point_in_pixel);
COMPOSITOR_EXPORT gfx::PointF ConvertPointToDIP(
    const Layer* layer,
    const gfx::PointF& point_in_pixel);
COMPOSITOR_EXPORT gfx::Size ConvertSizeToDIP(
    const Layer* layer,
    const gfx::Size& size_in_pixel);
COMPOSITOR_EXPORT gfx::Rect ConvertRectToDIP(
    const Layer* layer,
    const gfx::Rect& rect_in_pixel);
COMPOSITOR_EXPORT gfx::Point ConvertPointToPixel(
    const Layer* layer,
    const gfx::Point& point_in_dip);
COMPOSITOR_EXPORT gfx::Size ConvertSizeToPixel(
    const Layer* layer,
    const gfx::Size& size_in_dip);
COMPOSITOR_EXPORT gfx::Rect ConvertRectToPixel(
    const Layer* layer,
    const gfx::Rect& rect_in_dip);
}  // namespace ui

#endif  // UI_COMPOSITOR_DIP_UTIL_H_

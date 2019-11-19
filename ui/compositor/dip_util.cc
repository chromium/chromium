// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/dip_util.h"

#include "base/command_line.h"
#include "cc/layers/layer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"

#if DCHECK_IS_ON()
#include "ui/compositor/layer_animator.h"
#endif

namespace ui {

float GetDeviceScaleFactor(const Layer* layer) {
  return layer->device_scale_factor();
}

gfx::Point ConvertPointToDIP(const Layer* layer,
                             const gfx::Point& point_in_pixel) {
  return gfx::ConvertPointToDIP(GetDeviceScaleFactor(layer), point_in_pixel);
}

gfx::PointF ConvertPointToDIP(const Layer* layer,
                              const gfx::PointF& point_in_pixel) {
  return gfx::ConvertPointToDIP(GetDeviceScaleFactor(layer), point_in_pixel);
}

gfx::Size ConvertSizeToDIP(const Layer* layer,
                           const gfx::Size& size_in_pixel) {
  return gfx::ConvertSizeToDIP(GetDeviceScaleFactor(layer), size_in_pixel);
}

gfx::Rect ConvertRectToDIP(const Layer* layer,
                           const gfx::Rect& rect_in_pixel) {
  return gfx::ConvertRectToDIP(GetDeviceScaleFactor(layer), rect_in_pixel);
}

gfx::Point ConvertPointToPixel(const Layer* layer,
                               const gfx::Point& point_in_dip) {
  return gfx::ConvertPointToPixel(GetDeviceScaleFactor(layer), point_in_dip);
}

gfx::Size ConvertSizeToPixel(const Layer* layer,
                             const gfx::Size& size_in_dip) {
  return gfx::ConvertSizeToPixel(GetDeviceScaleFactor(layer), size_in_dip);
}

gfx::Rect ConvertRectToPixel(const Layer* layer,
                             const gfx::Rect& rect_in_dip) {
  return gfx::ConvertRectToPixel(GetDeviceScaleFactor(layer), rect_in_dip);
}

}  // namespace ui

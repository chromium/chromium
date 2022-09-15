// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/point.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_IOS)
#include <CoreGraphics/CoreGraphics.h>
#elif BUILDFLAG(IS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

#if BUILDFLAG(IS_WIN)
Point::Point(DWORD point) {
  POINTS points = MAKEPOINTS(point);
  x_ = points.x;
  y_ = points.y;
}

Point::Point(const POINT& point) : x_(point.x), y_(point.y) {
}

Point& Point::operator=(const POINT& point) {
  x_ = point.x;
  y_ = point.y;
  return *this;
}
#elif BUILDFLAG(IS_APPLE)
Point::Point(const CGPoint& point) : x_(point.x), y_(point.y) {
}
#endif

#if BUILDFLAG(IS_WIN)
POINT Point::ToPOINT() const {
  POINT p;
  p.x = x();
  p.y = y();
  return p;
}
#elif BUILDFLAG(IS_APPLE)
CGPoint Point::ToCGPoint() const {
  return CGPointMake(x(), y());
}
#endif

void Point::SetToMin(const Point& other) {
  x_ = std::min(x_, other.x_);
  y_ = std::min(y_, other.y_);
}

void Point::SetToMax(const Point& other) {
  x_ = std::max(x_, other.x_);
  y_ = std::max(y_, other.y_);
}

std::string Point::ToString() const {
  return base::StringPrintf("%d,%d", x(), y());
}

Point ScaleToCeiledPoint(const Point& point, float x_scale, float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return point;
  return ToCeiledPoint(ScalePoint(gfx::PointF(point), x_scale, y_scale));
}

Point ScaleToCeiledPoint(const Point& point, float scale) {
  if (scale == 1.f)
    return point;
  return ToCeiledPoint(ScalePoint(gfx::PointF(point), scale, scale));
}

Point ScaleToFlooredPoint(const Point& point, float x_scale, float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return point;
  return ToFlooredPoint(ScalePoint(gfx::PointF(point), x_scale, y_scale));
}

Point ScaleToFlooredPoint(const Point& point, float scale) {
  if (scale == 1.f)
    return point;
  return ToFlooredPoint(ScalePoint(gfx::PointF(point), scale, scale));
}

Point ScaleToRoundedPoint(const Point& point, float x_scale, float y_scale) {
  if (x_scale == 1.f && y_scale == 1.f)
    return point;
  return ToRoundedPoint(ScalePoint(gfx::PointF(point), x_scale, y_scale));
}

Point ScaleToRoundedPoint(const Point& point, float scale) {
  if (scale == 1.f)
    return point;
  return ToRoundedPoint(ScalePoint(gfx::PointF(point), scale, scale));
}

}  // namespace gfx

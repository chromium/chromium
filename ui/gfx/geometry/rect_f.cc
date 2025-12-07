// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/rect_f.h"

#include <algorithm>
#include <limits>

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/outsets_f.h"

#if BUILDFLAG(IS_IOS)
#include <CoreGraphics/CoreGraphics.h>
#elif BUILDFLAG(IS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

static void AdjustAlongAxis(float dst_origin,
                            float dst_size,
                            float* origin,
                            float* size) {
  *size = std::min(dst_size, *size);
  if (*origin < dst_origin)
    *origin = dst_origin;
  else
    *origin = std::min(dst_origin + dst_size, *origin + *size) - *size;
}

#if BUILDFLAG(IS_APPLE)
RectF::RectF(const CGRect& r)
    : origin_(r.origin.x, r.origin.y), size_(r.size.width, r.size.height) {
}

CGRect RectF::ToCGRect() const {
  return CGRectMake(x(), y(), width(), height());
}
#endif

void RectF::Inset(const InsetsF& insets) {
  origin_ += Vector2dF(insets.left(), insets.top());
  set_width(width() - insets.width());
  set_height(height() - insets.height());
}

void RectF::Offset(float horizontal, float vertical) {
  origin_ += Vector2dF(horizontal, vertical);
}

void RectF::operator+=(const Vector2dF& offset) {
  origin_ += offset;
}

void RectF::operator-=(const Vector2dF& offset) {
  origin_ -= offset;
}

InsetsF RectF::InsetsFrom(const RectF& inner) const {
  return InsetsF::TLBR(inner.y() - y(), inner.x() - x(),
                       bottom() - inner.bottom(), right() - inner.right());
}

bool RectF::operator<(const RectF& other) const {
  if (origin_ != other.origin_)
    return origin_ < other.origin_;

  if (width() == other.width())
    return height() < other.height();
  return width() < other.width();
}

bool RectF::Contains(float point_x, float point_y) const {
  return point_x >= x() && point_x < right() && point_y >= y() &&
         point_y < bottom();
}

bool RectF::InclusiveContains(float point_x, float point_y) const {
  return point_x >= x() && point_x <= right() && point_y >= y() &&
         point_y <= bottom();
}

bool RectF::Contains(const RectF& rect) const {
  return rect.x() >= x() && rect.right() <= right() && rect.y() >= y() &&
         rect.bottom() <= bottom();
}

bool RectF::Intersects(const RectF& rect) const {
  return !IsEmpty() && !rect.IsEmpty() && rect.x() < right() &&
         rect.right() > x() && rect.y() < bottom() && rect.bottom() > y();
}

void RectF::Intersect(const RectF& rect) {
  if (IsEmpty() || rect.IsEmpty()) {
    SetRect(0, 0, 0, 0);
    return;
  }

  float rx = std::max(x(), rect.x());
  float ry = std::max(y(), rect.y());
  float rr = std::min(right(), rect.right());
  float rb = std::min(bottom(), rect.bottom());

  if (rx >= rr || ry >= rb) {
    SetRect(0, 0, 0, 0);
    return;
  }

  SetRect(rx, ry, rr - rx, rb - ry);
}

bool RectF::InclusiveIntersect(const RectF& rect) {
  float rx = std::max(x(), rect.x());
  float ry = std::max(y(), rect.y());
  float rr = std::min(right(), rect.right());
  float rb = std::min(bottom(), rect.bottom());

  // Return a clean empty rectangle for non-intersecting cases.
  if (rx > rr || ry > rb) {
    SetRect(0, 0, 0, 0);
    return false;
  }

  SetRect(rx, ry, rr - rx, rb - ry);
  return true;
}

void RectF::Union(const RectF& rect) {
  if (IsEmpty()) {
    *this = rect;
    return;
  }
  if (rect.IsEmpty())
    return;

  UnionEvenIfEmpty(rect);
}

void RectF::UnionEvenIfEmpty(const RectF& rect) {
  float rx = std::min(x(), rect.x());
  float ry = std::min(y(), rect.y());
  float rr = std::max(right(), rect.right());
  float rb = std::max(bottom(), rect.bottom());

  SetRect(rx, ry, rr - rx, rb - ry);

  // Due to floating errors and SizeF::clamp(), the new rect may not fully
  // contain the original rects at the right/bottom side. Expand the rect in
  // the case.
  constexpr auto kFloatMax = std::numeric_limits<float>::max();
  if (right() < rr && width() < kFloatMax) [[unlikely]] {
    size_.SetToNextWidth();
    DCHECK_GE(right(), rr);
  }
  if (bottom() < rb && height() < kFloatMax) [[unlikely]] {
    size_.SetToNextHeight();
    DCHECK_GE(bottom(), rb);
  }
}

void RectF::Subtract(const RectF& rect) {
  if (!Intersects(rect))
    return;
  if (rect.Contains(*this)) {
    SetRect(0, 0, 0, 0);
    return;
  }

  float rx = x();
  float ry = y();
  float rr = right();
  float rb = bottom();

  if (rect.y() <= y() && rect.bottom() >= bottom()) {
    // complete intersection in the y-direction
    if (rect.x() <= x()) {
      rx = rect.right();
    } else if (rect.right() >= right()) {
      rr = rect.x();
    }
  } else if (rect.x() <= x() && rect.right() >= right()) {
    // complete intersection in the x-direction
    if (rect.y() <= y()) {
      ry = rect.bottom();
    } else if (rect.bottom() >= bottom()) {
      rb = rect.y();
    }
  }
  SetRect(rx, ry, rr - rx, rb - ry);
}

void RectF::AdjustToFit(const RectF& rect) {
  float new_x = x();
  float new_y = y();
  float new_width = width();
  float new_height = height();
  AdjustAlongAxis(rect.x(), rect.width(), &new_x, &new_width);
  AdjustAlongAxis(rect.y(), rect.height(), &new_y, &new_height);
  SetRect(new_x, new_y, new_width, new_height);
}

PointF RectF::CenterPoint() const {
  return PointF(x() + width() / 2, y() + height() / 2);
}

void RectF::ClampToCenteredSize(const SizeF& size) {
  float new_width = std::min(width(), size.width());
  float new_height = std::min(height(), size.height());
  float new_x = x() + (width() - new_width) / 2;
  float new_y = y() + (height() - new_height) / 2;
  SetRect(new_x, new_y, new_width, new_height);
}

void RectF::Transpose() {
  SetRect(y(), x(), height(), width());
}

void RectF::SplitVertically(RectF& left_half, RectF& right_half) const {
  left_half.SetRect(x(), y(), width() / 2, height());
  right_half.SetRect(left_half.right(), y(), width() - left_half.width(),
                     height());
}

void RectF::SplitHorizontally(RectF& top_half, RectF& bottom_half) const {
  top_half.SetRect(x(), y(), width(), height() / 2);
  bottom_half.SetRect(x(), top_half.bottom(), width(),
                      height() - top_half.height());
}

bool RectF::SharesEdgeWith(const RectF& rect) const {
  return (y() == rect.y() && height() == rect.height() &&
          (x() == rect.right() || right() == rect.x())) ||
         (x() == rect.x() && width() == rect.width() &&
          (y() == rect.bottom() || bottom() == rect.y()));
}

float RectF::ManhattanDistanceToPoint(const PointF& point) const {
  float x_distance =
      std::max<float>(0, std::max(x() - point.x(), point.x() - right()));
  float y_distance =
      std::max<float>(0, std::max(y() - point.y(), point.y() - bottom()));

  return x_distance + y_distance;
}

float RectF::ManhattanInternalDistance(const RectF& rect) const {
  RectF c(*this);
  c.Union(rect);

  static constexpr float kEpsilon = std::numeric_limits<float>::epsilon();
  float x = std::max(0.f, c.width() - width() - rect.width() + kEpsilon);
  float y = std::max(0.f, c.height() - height() - rect.height() + kEpsilon);
  return x + y;
}

PointF RectF::ClosestPoint(const PointF& point) const {
  return PointF(std::min(std::max(point.x(), x()), right()),
                std::min(std::max(point.y(), y()), bottom()));
}

bool RectF::IsExpressibleAsRect() const {
  return base::IsValueInRangeForNumericType<int>(x()) &&
         base::IsValueInRangeForNumericType<int>(y()) &&
         base::IsValueInRangeForNumericType<int>(width()) &&
         base::IsValueInRangeForNumericType<int>(height()) &&
         base::IsValueInRangeForNumericType<int>(right()) &&
         base::IsValueInRangeForNumericType<int>(bottom());
}

RectF IntersectRects(const RectF& a, const RectF& b) {
  RectF result = a;
  result.Intersect(b);
  return result;
}

RectF UnionRects(const RectF& a, const RectF& b) {
  RectF result = a;
  result.Union(b);
  return result;
}

RectF UnionRects(base::span<const RectF> rects) {
  RectF result;
  for (const RectF& rect : rects) {
    result.Union(rect);
  }
  return result;
}

RectF UnionRectsEvenIfEmpty(const RectF& a, const RectF& b) {
  RectF result = a;
  result.UnionEvenIfEmpty(b);
  return result;
}

RectF SubtractRects(const RectF& a, const RectF& b) {
  RectF result = a;
  result.Subtract(b);
  return result;
}

// Construct a rectangle with top-left corner at |p1| and bottom-right corner
// at |p2|. If the exact result of top - bottom or left - right cannot be
// presented in float, then the height/width will be grown to the next
// float, so that it includes both |p1| and |p2|.
RectF BoundingRect(const PointF& p1, const PointF& p2) {
  float left = std::min(p1.x(), p2.x());
  float top = std::min(p1.y(), p2.y());
  float right = std::max(p1.x(), p2.x());
  float bottom = std::max(p1.y(), p2.y());
  float width = right - left;
  float height = bottom - top;

  // If the precision is lost during the calculation, always grow to the next
  // value to include both ends.
  if (left + width != right) {
    width = std::nextafter((width), std::numeric_limits<float>::infinity());
    if (std::isinf(width)) {
      width = std::numeric_limits<float>::max();
    }
  }
  if (top + height != bottom) {
    height = std::nextafter((height), std::numeric_limits<float>::infinity());
    if (std::isinf(height)) {
      height = std::numeric_limits<float>::max();
    }
  }

  return RectF(left, top, width, height);
}

RectF MaximumCoveredRect(const RectF& a, const RectF& b) {
  // Check a or b by itself.
  RectF maximum = a;
  float maximum_area = a.size().GetArea();
  if (b.size().GetArea() > maximum_area) {
    maximum = b;
    maximum_area = b.size().GetArea();
  }
  // Check the regions that include the intersection of a and b. This can be
  // done by taking the intersection and expanding it vertically and
  // horizontally. These expanded intersections will both still be covered by
  // a or b.
  RectF intersection = a;
  intersection.InclusiveIntersect(b);
  if (!intersection.size().IsZero()) {
    RectF vert_expanded_intersection = intersection;
    vert_expanded_intersection.set_y(std::min(a.y(), b.y()));
    vert_expanded_intersection.set_height(std::max(a.bottom(), b.bottom()) -
                                          vert_expanded_intersection.y());
    if (vert_expanded_intersection.size().GetArea() > maximum_area) {
      maximum = vert_expanded_intersection;
      maximum_area = vert_expanded_intersection.size().GetArea();
    }
    RectF horiz_expanded_intersection(intersection);
    horiz_expanded_intersection.set_x(std::min(a.x(), b.x()));
    horiz_expanded_intersection.set_width(std::max(a.right(), b.right()) -
                                          horiz_expanded_intersection.x());
    if (horiz_expanded_intersection.size().GetArea() > maximum_area) {
      maximum = horiz_expanded_intersection;
      maximum_area = horiz_expanded_intersection.size().GetArea();
    }
  }
  return maximum;
}

RectF MapRect(const RectF& r, const RectF& src_rect, const RectF& dest_rect) {
  if (src_rect.IsEmpty())
    return RectF();

  float width_scale = dest_rect.width() / src_rect.width();
  float height_scale = dest_rect.height() / src_rect.height();
  return RectF(dest_rect.x() + (r.x() - src_rect.x()) * width_scale,
               dest_rect.y() + (r.y() - src_rect.y()) * height_scale,
               r.width() * width_scale, r.height() * height_scale);
}

std::string RectF::ToString() const {
  return base::StringPrintf("%s %s", origin().ToString().c_str(),
                            size().ToString().c_str());
}

bool RectF::ApproximatelyEqual(const RectF& rect,
                               float tolerance_x,
                               float tolerance_y) const {
  return std::abs(x() - rect.x()) <= tolerance_x &&
         std::abs(y() - rect.y()) <= tolerance_y &&
         std::abs(right() - rect.right()) <= tolerance_x &&
         std::abs(bottom() - rect.bottom()) <= tolerance_y;
}

}  // namespace gfx

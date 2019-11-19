// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_MOJOM_GEOMETRY_MOJOM_TRAITS_H_
#define UI_GFX_GEOMETRY_MOJOM_GEOMETRY_MOJOM_TRAITS_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-shared.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::InsetsDataView, gfx::Insets> {
  static int top(const gfx::Insets& p) { return p.top(); }
  static int left(const gfx::Insets& p) { return p.left(); }
  static int bottom(const gfx::Insets& p) { return p.bottom(); }
  static int right(const gfx::Insets& p) { return p.right(); }
  static bool Read(gfx::mojom::InsetsDataView data, gfx::Insets* out) {
    out->Set(data.top(), data.left(), data.bottom(), data.right());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::InsetsFDataView, gfx::InsetsF> {
  static float top(const gfx::InsetsF& p) { return p.top(); }
  static float left(const gfx::InsetsF& p) { return p.left(); }
  static float bottom(const gfx::InsetsF& p) { return p.bottom(); }
  static float right(const gfx::InsetsF& p) { return p.right(); }
  static bool Read(gfx::mojom::InsetsFDataView data, gfx::InsetsF* out) {
    out->Set(data.top(), data.left(), data.bottom(), data.right());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::PointDataView, gfx::Point> {
  static int x(const gfx::Point& p) { return p.x(); }
  static int y(const gfx::Point& p) { return p.y(); }
  static bool Read(gfx::mojom::PointDataView data, gfx::Point* out) {
    out->SetPoint(data.x(), data.y());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::PointFDataView, gfx::PointF> {
  static float x(const gfx::PointF& p) { return p.x(); }
  static float y(const gfx::PointF& p) { return p.y(); }
  static bool Read(gfx::mojom::PointFDataView data, gfx::PointF* out) {
    out->SetPoint(data.x(), data.y());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::Point3FDataView, gfx::Point3F> {
  static float x(const gfx::Point3F& p) { return p.x(); }
  static float y(const gfx::Point3F& p) { return p.y(); }
  static float z(const gfx::Point3F& p) { return p.z(); }
  static bool Read(gfx::mojom::Point3FDataView data, gfx::Point3F* out) {
    out->SetPoint(data.x(), data.y(), data.z());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::RectDataView, gfx::Rect> {
  static int x(const gfx::Rect& p) { return p.x(); }
  static int y(const gfx::Rect& p) { return p.y(); }
  static int width(const gfx::Rect& p) { return p.width(); }
  static int height(const gfx::Rect& p) { return p.height(); }
  static bool Read(gfx::mojom::RectDataView data, gfx::Rect* out) {
    if (data.width() < 0 || data.height() < 0)
      return false;

    out->SetRect(data.x(), data.y(), data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::RectFDataView, gfx::RectF> {
  static float x(const gfx::RectF& p) { return p.x(); }
  static float y(const gfx::RectF& p) { return p.y(); }
  static float width(const gfx::RectF& p) { return p.width(); }
  static float height(const gfx::RectF& p) { return p.height(); }
  static bool Read(gfx::mojom::RectFDataView data, gfx::RectF* out) {
    if (data.width() < 0 || data.height() < 0)
      return false;

    out->SetRect(data.x(), data.y(), data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::SizeDataView, gfx::Size> {
  static int width(const gfx::Size& p) { return p.width(); }
  static int height(const gfx::Size& p) { return p.height(); }
  static bool Read(gfx::mojom::SizeDataView data, gfx::Size* out) {
    if (data.width() < 0 || data.height() < 0)
      return false;

    out->SetSize(data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::SizeFDataView, gfx::SizeF> {
  static float width(const gfx::SizeF& p) { return p.width(); }
  static float height(const gfx::SizeF& p) { return p.height(); }
  static bool Read(gfx::mojom::SizeFDataView data, gfx::SizeF* out) {
    if (data.width() < 0 || data.height() < 0)
      return false;

    out->SetSize(data.width(), data.height());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::Vector2dDataView, gfx::Vector2d> {
  static int x(const gfx::Vector2d& v) { return v.x(); }
  static int y(const gfx::Vector2d& v) { return v.y(); }
  static bool Read(gfx::mojom::Vector2dDataView data, gfx::Vector2d* out) {
    out->set_x(data.x());
    out->set_y(data.y());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::Vector2dFDataView, gfx::Vector2dF> {
  static float x(const gfx::Vector2dF& v) { return v.x(); }
  static float y(const gfx::Vector2dF& v) { return v.y(); }
  static bool Read(gfx::mojom::Vector2dFDataView data, gfx::Vector2dF* out) {
    out->set_x(data.x());
    out->set_y(data.y());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::Vector3dFDataView, gfx::Vector3dF> {
  static float x(const gfx::Vector3dF& v) { return v.x(); }
  static float y(const gfx::Vector3dF& v) { return v.y(); }
  static float z(const gfx::Vector3dF& v) { return v.z(); }
  static bool Read(gfx::mojom::Vector3dFDataView data, gfx::Vector3dF* out) {
    out->set_x(data.x());
    out->set_y(data.y());
    out->set_z(data.z());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::ScrollOffsetDataView, gfx::ScrollOffset> {
  static float x(const gfx::ScrollOffset& v) { return v.x(); }
  static float y(const gfx::ScrollOffset& v) { return v.y(); }
  static bool Read(gfx::mojom::ScrollOffsetDataView data,
                   gfx::ScrollOffset* out) {
    out->set_x(data.x());
    out->set_y(data.y());
    return true;
  }
};

template <>
struct StructTraits<gfx::mojom::QuaternionDataView, gfx::Quaternion> {
  static double x(const gfx::Quaternion& q) { return q.x(); }
  static double y(const gfx::Quaternion& q) { return q.y(); }
  static double z(const gfx::Quaternion& q) { return q.z(); }
  static double w(const gfx::Quaternion& q) { return q.w(); }
  static bool Read(gfx::mojom::QuaternionDataView data, gfx::Quaternion* out) {
    out->set_x(data.x());
    out->set_y(data.y());
    out->set_z(data.z());
    out->set_w(data.w());
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_GEOMETRY_MOJOM_GEOMETRY_MOJOM_TRAITS_H_

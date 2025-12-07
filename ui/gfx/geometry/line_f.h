// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_LINE_F_H_
#define UI_GFX_GEOMETRY_LINE_F_H_

#include <optional>
#include <utility>

#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {

struct LineF {
  PointF p1;
  PointF p2;

  constexpr Vector2dF Normal() const {
    return gfx::Vector2dF(p1.y() - p2.y(), p2.x() - p1.x());
  }

  inline std::optional<gfx::PointF> IntersectionWith(
      const gfx::LineF& other) const {
    const Vector2dF a_length = p2 - p1;
    const Vector2dF b_length = other.p2 - other.p1;
    const float denom = CrossProduct(a_length, b_length);
    if (!denom) {
      return std::nullopt;
    }

    const float param = CrossProduct(other.p1 - p1, b_length) / denom;
    return p1 + ScaleVector2d(a_length, param);
  }
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_LINE_F_H_

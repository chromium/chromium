// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_RRECT_F_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_RRECT_F_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/rrect_f_builder.h"
#include "ui/gfx/mojom/rrect_f.mojom-shared.h"

namespace mojo {

namespace {

gfx::mojom::RRectFType GfxRRectFTypeToMojo(gfx::RRectF::Type type) {
  switch (type) {
    case gfx::RRectF::Type::kEmpty:
      return gfx::mojom::RRectFType::kEmpty;
    case gfx::RRectF::Type::kRect:
      return gfx::mojom::RRectFType::kRect;
    case gfx::RRectF::Type::kSingle:
      return gfx::mojom::RRectFType::kSingle;
    case gfx::RRectF::Type::kSimple:
      return gfx::mojom::RRectFType::kSimple;
    case gfx::RRectF::Type::kOval:
      return gfx::mojom::RRectFType::kOval;
    case gfx::RRectF::Type::kComplex:
      return gfx::mojom::RRectFType::kComplex;
  }
  NOTREACHED_IN_MIGRATION();
  return gfx::mojom::RRectFType::kEmpty;
}

gfx::RRectF::Type MojoRRectFTypeToGfx(gfx::mojom::RRectFType type) {
  switch (type) {
    case gfx::mojom::RRectFType::kEmpty:
      return gfx::RRectF::Type::kEmpty;
    case gfx::mojom::RRectFType::kRect:
      return gfx::RRectF::Type::kRect;
    case gfx::mojom::RRectFType::kSingle:
      return gfx::RRectF::Type::kSingle;
    case gfx::mojom::RRectFType::kSimple:
      return gfx::RRectF::Type::kSimple;
    case gfx::mojom::RRectFType::kOval:
      return gfx::RRectF::Type::kOval;
    case gfx::mojom::RRectFType::kComplex:
      return gfx::RRectF::Type::kComplex;
  }
  NOTREACHED_IN_MIGRATION();
  return gfx::RRectF::Type::kEmpty;
}

}  // namespace

template <>
struct StructTraits<gfx::mojom::RRectFDataView, gfx::RRectF> {
  static gfx::mojom::RRectFType type(const gfx::RRectF& input) {
    return GfxRRectFTypeToMojo(input.GetType());
  }

  static gfx::RectF rect(const gfx::RRectF& input) { return input.rect(); }

  static gfx::Vector2dF upper_left(const gfx::RRectF& input) {
    return input.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft);
  }

  static gfx::Vector2dF upper_right(const gfx::RRectF& input) {
    return input.GetCornerRadii(gfx::RRectF::Corner::kUpperRight);
  }

  static gfx::Vector2dF lower_right(const gfx::RRectF& input) {
    return input.GetCornerRadii(gfx::RRectF::Corner::kLowerRight);
  }

  static gfx::Vector2dF lower_left(const gfx::RRectF& input) {
    return input.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft);
  }

  static bool Read(gfx::mojom::RRectFDataView data, gfx::RRectF* out) {
    gfx::RRectF::Type type(MojoRRectFTypeToGfx(data.type()));
    gfx::RectF rect;
    if (!data.ReadRect(&rect))
      return false;
    if (type <= gfx::RRectF::Type::kRect) {
      *out = gfx::RRectFBuilder().set_rect(rect).Build();
      return true;
    }
    gfx::Vector2dF upper_left;
    if (!data.ReadUpperLeft(&upper_left))
      return false;
    if (type <= gfx::RRectF::Type::kSimple) {
      *out = gfx::RRectFBuilder()
                 .set_rect(rect)
                 .set_radius(upper_left.x(), upper_left.y())
                 .Build();
      return true;
    }
    gfx::Vector2dF upper_right;
    gfx::Vector2dF lower_right;
    gfx::Vector2dF lower_left;
    if (!data.ReadUpperRight(&upper_right) ||
        !data.ReadLowerRight(&lower_right) ||
        !data.ReadLowerLeft(&lower_left)) {
      return false;
    }
    *out = gfx::RRectFBuilder()
               .set_rect(rect)
               .set_upper_left(upper_left.x(), upper_left.y())
               .set_upper_right(upper_right.x(), upper_right.y())
               .set_lower_right(lower_right.x(), lower_right.y())
               .set_lower_left(lower_left.x(), lower_left.y())
               .Build();
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_RRECT_F_MOJOM_TRAITS_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_SELECTION_BOUND_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_SELECTION_BOUND_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/selection_bound.mojom-shared.h"
#include "ui/gfx/selection_bound.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::SelectionBoundType, gfx::SelectionBound::Type> {
  static gfx::mojom::SelectionBoundType ToMojom(
      gfx::SelectionBound::Type input) {
    switch (input) {
      case gfx::SelectionBound::LEFT:
        return gfx::mojom::SelectionBoundType::LEFT;
      case gfx::SelectionBound::RIGHT:
        return gfx::mojom::SelectionBoundType::RIGHT;
      case gfx::SelectionBound::CENTER:
        return gfx::mojom::SelectionBoundType::CENTER;
      case gfx::SelectionBound::HIDDEN:
        return gfx::mojom::SelectionBoundType::HIDDEN;
      case gfx::SelectionBound::EMPTY:
        return gfx::mojom::SelectionBoundType::EMPTY;
    }
    NOTREACHED();
  }

  static bool FromMojom(gfx::mojom::SelectionBoundType input,
                        gfx::SelectionBound::Type* out) {
    switch (input) {
      case gfx::mojom::SelectionBoundType::LEFT:
        *out = gfx::SelectionBound::LEFT;
        return true;
      case gfx::mojom::SelectionBoundType::RIGHT:
        *out = gfx::SelectionBound::RIGHT;
        return true;
      case gfx::mojom::SelectionBoundType::CENTER:
        *out = gfx::SelectionBound::CENTER;
        return true;
      case gfx::mojom::SelectionBoundType::HIDDEN:
        *out = gfx::SelectionBound::HIDDEN;
        return true;
      case gfx::mojom::SelectionBoundType::EMPTY:
        *out = gfx::SelectionBound::EMPTY;
        return true;
    }
    NOTREACHED();
  }
};

template <>
struct StructTraits<gfx::mojom::SelectionBoundDataView, gfx::SelectionBound> {
  static gfx::SelectionBound::Type type(const gfx::SelectionBound& input) {
    return input.type();
  }

  static gfx::PointF edge_start(const gfx::SelectionBound& input) {
    return input.edge_start();
  }

  static gfx::PointF edge_end(const gfx::SelectionBound& input) {
    return input.edge_end();
  }

  static gfx::PointF visible_edge_start(const gfx::SelectionBound& input) {
    return input.visible_edge_start();
  }

  static gfx::PointF visible_edge_end(const gfx::SelectionBound& input) {
    return input.visible_edge_end();
  }

  static bool visible(const gfx::SelectionBound& input) {
    return input.visible();
  }

  static bool Read(gfx::mojom::SelectionBoundDataView data,
                   gfx::SelectionBound* out) {
    gfx::SelectionBound::Type type;
    gfx::PointF edge_start;
    gfx::PointF edge_end;
    gfx::PointF visible_edge_start;
    gfx::PointF visible_edge_end;
    if (!data.ReadType(&type) || !data.ReadEdgeStart(&edge_start) ||
        !data.ReadEdgeEnd(&edge_end) ||
        !data.ReadVisibleEdgeStart(&visible_edge_start) ||
        !data.ReadVisibleEdgeEnd(&visible_edge_end)) {
      return false;
    }
    out->SetEdge(edge_start, edge_end);
    out->SetVisibleEdge(visible_edge_start, visible_edge_end);
    out->set_type(type);
    out->set_visible(data.visible());
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_SELECTION_BOUND_MOJOM_TRAITS_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJO_SELECTION_BOUND_STRUCT_TRAITS_H_
#define UI_GFX_MOJO_SELECTION_BOUND_STRUCT_TRAITS_H_

#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "ui/gfx/mojo/selection_bound.mojom-shared.h"
#include "ui/gfx/selection_bound.h"

namespace mojo {

namespace {

gfx::mojom::SelectionBoundType GfxSelectionBoundTypeToMojo(
    gfx::SelectionBound::Type type) {
  switch (type) {
    case gfx::SelectionBound::LEFT:
      return gfx::mojom::SelectionBoundType::LEFT;
    case gfx::SelectionBound::RIGHT:
      return gfx::mojom::SelectionBoundType::RIGHT;
    case gfx::SelectionBound::CENTER:
      return gfx::mojom::SelectionBoundType::CENTER;
    case gfx::SelectionBound::EMPTY:
      return gfx::mojom::SelectionBoundType::EMPTY;
  }
  NOTREACHED();
  return gfx::mojom::SelectionBoundType::EMPTY;
}

gfx::SelectionBound::Type MojoSelectionBoundTypeToGfx(
    gfx::mojom::SelectionBoundType type) {
  switch (type) {
    case gfx::mojom::SelectionBoundType::LEFT:
      return gfx::SelectionBound::LEFT;
    case gfx::mojom::SelectionBoundType::RIGHT:
      return gfx::SelectionBound::RIGHT;
    case gfx::mojom::SelectionBoundType::CENTER:
      return gfx::SelectionBound::CENTER;
    case gfx::mojom::SelectionBoundType::EMPTY:
      return gfx::SelectionBound::EMPTY;
  }
  NOTREACHED();
  return gfx::SelectionBound::EMPTY;
}

}

template <>
struct StructTraits<gfx::mojom::SelectionBoundDataView, gfx::SelectionBound> {
  static gfx::mojom::SelectionBoundType type(const gfx::SelectionBound& input) {
    return GfxSelectionBoundTypeToMojo(input.type());
  }

  static gfx::PointF edge_top(const gfx::SelectionBound& input) {
    return input.edge_top();
  }

  static gfx::PointF edge_bottom(const gfx::SelectionBound& input) {
    return input.edge_bottom();
  }

  static bool visible(const gfx::SelectionBound& input) {
    return input.visible();
  }

  static bool Read(gfx::mojom::SelectionBoundDataView data,
                   gfx::SelectionBound* out) {
    gfx::PointF edge_top;
    gfx::PointF edge_bottom;
    if (!data.ReadEdgeTop(&edge_top) || !data.ReadEdgeBottom(&edge_bottom))
      return false;
    out->SetEdge(edge_top, edge_bottom);
    out->set_type(MojoSelectionBoundTypeToGfx(data.type()));
    out->set_visible(data.visible());
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJO_SELECTION_BOUND_STRUCT_TRAITS_H_

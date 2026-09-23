// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DRAGGABLE_REGION_H_
#define UI_GFX_DRAGGABLE_REGION_H_

#include <iterator>
#include <vector>

#include "skia/ext/region_ops.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace gfx {

// Builds the window-drag hit-test region from the app-region rects a page
// reported: draggable rects are added and no-drag rects are removed, in list
// order, so later rects win where they overlap. `regions` is a range of
// pointer-like elements with `bounds` (a gfx::Rect) and `draggable` (a bool),
// such as std::vector<blink::mojom::DraggableRegionPtr>; it is a template so
// that ui/gfx does not depend on that type.
template <typename DraggableRegions>
SkRegion DraggableRegionsToSkRegion(const DraggableRegions& regions) {
  std::vector<skia::RegionRectOp> ops;
  ops.reserve(std::size(regions));
  for (const auto& region : regions) {
    ops.push_back({RectToSkIRect(region->bounds), !region->draggable});
  }
  return skia::RegionFromRectOps(ops);
}

}  // namespace gfx

#endif  // UI_GFX_DRAGGABLE_REGION_H_

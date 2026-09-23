// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/draggable_region.h"

#include <memory>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
namespace {

// Same shape as blink::mojom::DraggableRegion, held by pointer like the mojom
// StructPtr.
struct Region {
  Rect bounds;
  bool draggable = false;
};

std::unique_ptr<Region> MakeRegion(const Rect& bounds, bool draggable) {
  auto region = std::make_unique<Region>();
  region->bounds = bounds;
  region->draggable = draggable;
  return region;
}

TEST(DraggableRegionTest, Empty) {
  EXPECT_TRUE(DraggableRegionsToSkRegion(std::vector<std::unique_ptr<Region>>())
                  .isEmpty());
}

TEST(DraggableRegionTest, LaterRectsWin) {
  std::vector<std::unique_ptr<Region>> regions;
  regions.push_back(MakeRegion(Rect(0, 0, 100, 20), true));
  regions.push_back(MakeRegion(Rect(40, 0, 20, 20), false));
  SkRegion region = DraggableRegionsToSkRegion(regions);
  EXPECT_TRUE(region.contains(10, 10));
  EXPECT_FALSE(region.contains(50, 10));
  EXPECT_TRUE(region.contains(90, 10));

  // A no-drag rect before any drag rect removes nothing.
  regions.clear();
  regions.push_back(MakeRegion(Rect(40, 0, 20, 20), false));
  regions.push_back(MakeRegion(Rect(0, 0, 100, 20), true));
  EXPECT_TRUE(DraggableRegionsToSkRegion(regions) ==
              SkRegion(SkIRect::MakeWH(100, 20)));
}

}  // namespace
}  // namespace gfx

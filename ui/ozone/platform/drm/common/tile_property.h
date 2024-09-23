// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_TILE_PROPERTY_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_TILE_PROPERTY_H_

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

// Properties of a tiled display, comprised of multiple tiles each with their
// own connector. Tiles are typically individual physical panels that are laid
// out in a specific way (usually in a single phsyical enclosure) to create a
// single, tiled display.
//
// For example, below is a representation of a tiled display with a 2x2
// |tile_layout|:
//
//                   |tile_size|.w
//                    +---------+
//
//               +    +---------+---------+    +
//               |    |         |         |    |
// |tile_size|.h |    |  (0,0)  |  (1,0)  |    |
//               |    |         |         |    |
//               +    +---------+---------+    +  |tile_size|.h
//                    |         |         |    |  * |tile_layout|.h
//                    |  (0,1)  |  (1,1)  |    |
//                    |         |         |    |
//                    +---------+---------+    +
//
//                    +---------+---------+
//              |total_size|.w * |tile_layout|.w
//
// Where |location| of each tiles are represented by (x, y) in each of the boxes
// above.

struct TileProperty {
  // ID that group tiles belonging to the same display.
  int group_id = 0;

  // If true, contents of the tile will scale to fit the entire display
  // when this tile is the only tile being transmitted.
  // Described as one of the "tile capabilities" in the DisplayID tiled display
  // blocks (both v1.3 and v2.0).
  bool scale_to_fit_display = false;

  // Resolution of the individual tile (not the entire tiled display).
  gfx::Size tile_size;

  // Dimensions of the tiles in the group.
  gfx::Size tile_layout;

  // The physical location of the tile in the display where tiles are laid out
  // in a grid described by |tile_layout|. The top-leftmost tile is (0,0).
  gfx::Point location;
};
}  // namespace ui
#endif  // UI_OZONE_PLATFORM_DRM_COMMON_TILE_PROPERTY_H_

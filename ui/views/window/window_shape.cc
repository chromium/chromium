// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/window_shape.h"

#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/size.h"

namespace views {

void GetDefaultWindowMask(const gfx::Size& size, SkPath* window_mask) {
  const SkScalar width = SkIntToScalar(size.width());
  const SkScalar height = SkIntToScalar(size.height());

  *window_mask = SkPath::Polygon(
      {
          {0, 3},
          {1, 3},
          {1, 1},
          {3, 1},
          {3, 0},

          {width - 3, 0},
          {width - 3, 1},
          {width - 1, 1},
          {width - 1, 3},
          {width, 3},

          {width, height - 3},
          {width - 1, height - 3},
          {width - 1, height - 1},
          {width - 3, height - 1},
          {width - 3, height},

          {3, height},
          {3, height - 1},
          {1, height - 1},
          {1, height - 3},
          {0, height - 3},
      },
      /*isClosed=*/true);
}

}  // namespace views

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox_util.h"

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"

namespace views {

const int kComboboxArrowPaddingWidth = 8;
const int kComboboxArrowContainerWidth =
    ComboboxArrowSize().width() + kComboboxArrowPaddingWidth * 2;

void PaintComboboxArrow(SkColor color,
                        const gfx::Rect& bounds,
                        gfx::Canvas* canvas) {
  // Since this is a core piece of UI and vector icons don't handle fractional
  // scale factors particularly well, manually draw an arrow and make sure it
  // looks good at all scale factors.
  float dsf = canvas->UndoDeviceScaleFactor();
  SkScalar x = std::ceil(bounds.x() * dsf);
  SkScalar y = std::ceil(bounds.y() * dsf);
  SkScalar height = std::floor(bounds.height() * dsf);
  SkPath path;
  // This epsilon makes sure that all the aliasing pixels are slightly more
  // than half full. Otherwise, rounding issues cause some to be considered
  // slightly less than half full and come out a little lighter.
  constexpr SkScalar kEpsilon = 0.0001f;
  path.moveTo(x - kEpsilon, y);
  path.rLineTo(/*x=*/height, /*y=*/height);
  path.rLineTo(/*x=*/2 * kEpsilon, /*y=*/0);
  path.rLineTo(/*x=*/height, /*y=*/-height);
  path.close();
  cc::PaintFlags flags;
  flags.setColor(color);
  flags.setAntiAlias(true);
  canvas->DrawPath(path, flags);
}

}  // namespace views

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox_util.h"

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/layout/layout_provider.h"

namespace views {

const int kComboboxArrowPaddingWidth = 8;
const int kComboboxArrowPaddingWidthChromeRefresh2023 = 4;

int GetComboboxArrowContainerWidthAndMargins() {
  // For ChromeRefresh2023, add extra margins between combobox arrow container
  // and edge of the combobox.
  return features::IsChromeRefresh2023()
             ? GetComboboxArrowContainerWidth() +
                   LayoutProvider::Get()->GetDistanceMetric(
                       DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING)
             : GetComboboxArrowContainerWidth();
}

int GetComboboxArrowContainerWidth() {
  int padding = features::IsChromeRefresh2023()
                    ? kComboboxArrowPaddingWidthChromeRefresh2023 * 2
                    : kComboboxArrowPaddingWidth * 2;
  return ComboboxArrowSize().width() + padding;
}

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
  path.rLineTo(/*dx=*/height, /*dy=*/height);
  path.rLineTo(/*dx=*/2 * kEpsilon, /*dy=*/0);
  path.rLineTo(/*dx=*/height, /*dy=*/-height);
  path.close();
  cc::PaintFlags flags;
  flags.setColor(color);
  flags.setAntiAlias(true);
  canvas->DrawPath(path, flags);
}

}  // namespace views

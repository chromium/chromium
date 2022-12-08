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
const int kComboboxArrowContainerWidth =
    ComboboxArrowSize().width() + kComboboxArrowPaddingWidth * 2;

int GetComboboxArrowContainerWidthAndMargins() {
  // Since the container will be visible, we need some extra margins for the
  // ChromeRefresh case.
  // TODO(crbug.com/1392549): Replace placeholder values and potentially combine
  // use cases with INSETS_VECTOR_IMAGE_BUTTON.
  gfx::Insets margins_chromerefresh2023 =
      LayoutProvider::Get()->GetInsetsMetric(INSETS_VECTOR_IMAGE_BUTTON);
  return features::IsChromeRefresh2023()
             ? kComboboxArrowContainerWidth + margins_chromerefresh2023.left() +
                   margins_chromerefresh2023.right()
             : kComboboxArrowContainerWidth;
}

void PaintComboboxArrowBackground(SkColor color,
                                  gfx::Canvas* canvas,
                                  gfx::PointF origin) {
  cc::PaintFlags flags;
  gfx::RectF background_bounds(
      origin,
      gfx::SizeF(kComboboxArrowContainerWidth, kComboboxArrowContainerWidth));
  flags.setColor(color);
  // TODO(crbug.com/1392549): Replace placeholder value for corner radius.
  int background_corner_radius_chromerefresh2023 =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kLow);
  canvas->DrawRoundRect(background_bounds,
                        background_corner_radius_chromerefresh2023, flags);
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

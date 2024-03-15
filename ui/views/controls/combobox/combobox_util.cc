// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox_util.h"

#include <memory>

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

namespace views {

const int kComboboxArrowPaddingWidth = 4;

int GetComboboxArrowContainerWidthAndMargins() {
  // add extra margins between combobox arrow container and edge of the
  // combobox.
  return GetComboboxArrowContainerWidth() +
         LayoutProvider::Get()->GetDistanceMetric(
             DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING);
}

int GetComboboxArrowContainerWidth() {
  return ComboboxArrowSize().width() + kComboboxArrowPaddingWidth * 2;
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

void ConfigureComboboxButtonInkDrop(Button* host_view) {
  // Colors already have opacity applied, so use opaque ink drops.
  constexpr float kOpaque = 1;

  InkDrop::Get(host_view)->SetMode(views::InkDropHost::InkDropMode::ON);
  host_view->SetHasInkDropActionOnClick(true);
  // We must use UseInkDropForFloodFillRipple here because
  // UseInkDropForSquareRipple hides the InkDrop when the ripple effect is
  // active instead of layering underneath it, causing flashing.
  InkDrop::UseInkDropForFloodFillRipple(InkDrop::Get(host_view),
                                        /*highlight_on_hover=*/true);
  InkDrop::Get(host_view)->SetHighlightOpacity(kOpaque);
  views::InkDrop::Get(host_view)->SetBaseColorCallback(base::BindRepeating(
      [](Button* host) {
        return color_utils::DeriveDefaultIconColor(
            host->GetColorProvider()->GetColor(
                ui::kColorComboboxInkDropHovered));
      },
      host_view));
  InkDrop::Get(host_view)->SetCreateRippleCallback(base::BindRepeating(
      [](Button* host, float opacity) -> std::unique_ptr<views::InkDropRipple> {
        return std::make_unique<views::FloodFillInkDropRipple>(
            InkDrop::Get(host), host->size(),
            InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
            host->GetColorProvider()->GetColor(ui::kColorComboboxInkDropRipple),
            opacity);
      },
      host_view, kOpaque));
}

}  // namespace views

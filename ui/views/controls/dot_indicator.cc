// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/dot_indicator.h"

#include <algorithm>
#include <utility>

#include "base/memory/ptr_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/cascading_property.h"

namespace views {

DotIndicator::~DotIndicator() = default;

// static
DotIndicator* DotIndicator::Install(View* parent) {
  auto dot = base::WrapUnique<DotIndicator>(new DotIndicator());
  dot->SetPaintToLayer();
  dot->layer()->SetFillsBoundsOpaquely(false);
  dot->SetVisible(false);
  return parent->AddChildView(std::move(dot));
}

void DotIndicator::SetColor(SkColor dot_color, SkColor border_color) {
  dot_color_ = dot_color;
  border_color_ = border_color;
  SchedulePaint();
}

void DotIndicator::Show() {
  SetVisible(true);
}

void DotIndicator::Hide() {
  SetVisible(false);
}

DotIndicator::DotIndicator() {
  // Don't allow the view to process events.
  SetCanProcessEventsWithinSubtree(false);
}

void DotIndicator::OnPaint(gfx::Canvas* canvas) {
  canvas->SaveLayerAlpha(SK_AlphaOPAQUE);

  DCHECK_EQ(width(), height());
  float radius = width() / 2.0f;
  const float scale = canvas->UndoDeviceScaleFactor();
  const int kStrokeWidthPx = 1;
  gfx::PointF center = gfx::RectF(GetLocalBounds()).CenterPoint();
  center.Scale(scale);

  // Fill the center.
  cc::PaintFlags flags;
  flags.setColor(dot_color_.value_or(GetCascadingAccentColor(this)));
  flags.setAntiAlias(true);
  canvas->DrawCircle(center, scale * radius - kStrokeWidthPx, flags);

  // Draw the border.
  flags.setColor(border_color_.value_or(GetCascadingBackgroundColor(this)));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kStrokeWidthPx * scale);
  canvas->DrawCircle(center, scale * radius - kStrokeWidthPx / 2.0f, flags);
}

void DotIndicator::OnThemeChanged() {
  View::OnThemeChanged();
  SchedulePaint();
}

BEGIN_METADATA(DotIndicator)
END_METADATA

}  // namespace views

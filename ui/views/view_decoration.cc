// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_decoration.h"

#include <memory>
#include <optional>
#include <utility>

#include "ui/compositor/layer.h"
#include "ui/decoration/highlight_border.h"
#include "ui/decoration/shadow.h"
#include "ui/decoration/shadow_with_highlight_border.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

ViewDecoration::ViewDecoration(View* view)
    : view_(view),
      decoration_(ui::Decoration::CreateShadowWithHighlightBorder()),
      source_(decoration_
                  ->GetSourceAs<ui::decoration::ShadowWithHighlightBorder>()) {
  if (!view_->layer()) {
    view_->SetPaintToLayer();
  }
  MoveLayerToRegion(LayerRegion::kBelow);
  view_observation_.Observe(view_);
}

ViewDecoration::~ViewDecoration() {
  if (view_) {
    OnViewIsDeleting(view_);
  }
}

void ViewDecoration::SetShadow(
    int elevation,
    std::optional<ui::decoration::Shadow::Style> style,
    std::optional<ui::decoration::Shadow::ElevationToColorsMap> color_map) {
  if (!source_) {
    return;
  }

  if (auto* existing_shadow = source_->shadow()) {
    if (style.has_value()) {
      existing_shadow->SetStyle(*style);
    }
    if (color_map.has_value()) {
      existing_shadow->SetColorMap(*color_map);
    }
    existing_shadow->SetElevation(elevation);
    return;
  }

  source_->SetShadow(std::make_unique<ui::decoration::Shadow>(
      elevation, style.value_or(ui::decoration::Shadow::Style::kMaterialDesign),
      std::move(color_map).value_or(
          ui::decoration::Shadow::ElevationToColorsMap())));
}

void ViewDecoration::ClearShadow() {
  if (source_) {
    source_->SetShadow(nullptr);
  }
}

ui::decoration::Shadow* ViewDecoration::shadow() {
  return source_ ? source_->shadow() : nullptr;
}

const ui::decoration::Shadow* ViewDecoration::shadow() const {
  return source_ ? source_->shadow() : nullptr;
}

void ViewDecoration::SetHighlightBorder(SkColor highlight_color,
                                        SkColor border_color,
                                        int thickness) {
  if (!source_) {
    return;
  }

  if (auto* existing_border = source_->highlight_border()) {
    existing_border->SetColors(highlight_color, border_color);
    existing_border->SetThickness(thickness);
    return;
  }

  source_->SetHighlightBorder(std::make_unique<ui::decoration::HighlightBorder>(
      highlight_color, border_color, thickness));
  MoveLayerToRegion(LayerRegion::kAbove);
}

void ViewDecoration::ClearHighlightBorder() {
  if (!source_ || !source_->highlight_border()) {
    return;
  }

  source_->SetHighlightBorder(nullptr);
  MoveLayerToRegion(LayerRegion::kBelow);
}

ui::decoration::HighlightBorder* ViewDecoration::highlight_border() {
  return source_ ? source_->highlight_border() : nullptr;
}

const ui::decoration::HighlightBorder* ViewDecoration::highlight_border()
    const {
  return source_ ? source_->highlight_border() : nullptr;
}

void ViewDecoration::SetRoundedCornerRadius(int corner_radius) {
  SetRoundedCorners(gfx::RoundedCornersF(corner_radius));
}

void ViewDecoration::SetRoundedCorners(const gfx::RoundedCornersF& radii) {
  if (decoration_) {
    decoration_->SetRoundedCorners(radii);
  }
}

void ViewDecoration::MoveLayerToRegion(LayerRegion region) {
  view_->RemoveLayerFromRegionsKeepInLayerTree(decoration_->layer());
  view_->AddLayerToRegion(decoration_->layer(), region);
  decoration_->SetContentBounds(view_->layer()->bounds());
}

void ViewDecoration::OnViewLayerBoundsSet(View* view) {
  decoration_->SetContentBounds(view_->layer()->bounds());
}

void ViewDecoration::OnViewIsDeleting(View* view) {
  source_ = nullptr;
  decoration_.reset();
  view_observation_.Reset();
  view_ = nullptr;
}

}  // namespace views

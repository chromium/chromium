// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_shadow.h"

#include "ui/compositor/layer.h"
#include "ui/decoration/shadow.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

ViewShadow::ViewShadow(View* view, int elevation)
    : view_(view), decoration_(ui::Decoration::CreateShadow(elevation)) {
  if (!view_->layer()) {
    view_->SetPaintToLayer();
  }
  view_->AddLayerToRegion(decoration_->layer(), LayerRegion::kBelow);
  decoration_->SetContentBounds(view_->layer()->bounds());
  view_observation_.Observe(view_);
}

ViewShadow::~ViewShadow() {
  if (view_) {
    OnViewIsDeleting(view_);
  }
}

void ViewShadow::SetRoundedCornerRadius(int corner_radius) {
  SetRoundedCorners(gfx::RoundedCornersF(corner_radius));
}

void ViewShadow::SetRoundedCorners(const gfx::RoundedCornersF& radii) {
  decoration_->SetRoundedCorners(radii);
}

void ViewShadow::OnViewLayerBoundsSet(View* view) {
  decoration_->SetContentBounds(view_->layer()->bounds());
}

void ViewShadow::OnViewIsDeleting(View* view) {
  decoration_.reset();
  view_observation_.Reset();
  view_ = nullptr;
}

}  // namespace views

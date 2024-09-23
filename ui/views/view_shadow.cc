// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_shadow.h"

#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

ViewShadow::ViewShadow(View* view, int elevation)
    : view_(view), shadow_(std::make_unique<ui::Shadow>()) {
  if (!view_->layer())
    view_->SetPaintToLayer();
  shadow_->Init(elevation);
  view_->AddLayerToRegion(shadow_->layer(), LayerRegion::kBelow);
  shadow_->SetContentBounds(view_->layer()->bounds());
  view_observation_.Observe(view_);
  shadow_observation_.Observe(shadow_.get());
}

ViewShadow::~ViewShadow() {
  if (view_)
    OnViewIsDeleting(view_);
}

void ViewShadow::SetRoundedCornerRadius(int corner_radius) {
  shadow_->SetRoundedCornerRadius(corner_radius);
}

void ViewShadow::OnLayerRecreated(ui::Layer* old_layer) {
  if (!view_)
    return;
  view_->RemoveLayerFromRegionsKeepInLayerTree(old_layer);
  view_->AddLayerToRegion(shadow_->layer(), LayerRegion::kBelow);
}

void ViewShadow::OnViewLayerBoundsSet(View* view) {
  shadow_->SetContentBounds(view->layer()->bounds());
}

void ViewShadow::OnViewIsDeleting(View* view) {
  shadow_observation_.Reset();
  shadow_.reset();
  view_observation_.Reset();
  view_ = nullptr;
}

}  // namespace views

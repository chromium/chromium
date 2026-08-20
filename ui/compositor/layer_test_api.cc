// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_test_api.h"

#include <algorithm>
#include <utility>

#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_mirror.h"
#include "ui/compositor/layer_solid_color.h"
#include "ui/compositor/layer_textured.h"

namespace ui {

LayerTestApi::LayerTestApi(Layer* layer) : layer_(layer) {}

LayerTestApi::~LayerTestApi() = default;

cc::MirrorLayer* LayerTestApi::mirror_layer() {
  CHECK(layer_->AsSolidColor());
  return layer_->AsSolidColor()->mirror_layer_.get();
}

const cc::MirrorLayer* LayerTestApi::mirror_layer() const {
  CHECK(layer_->AsSolidColor());
  return layer_->AsSolidColor()->mirror_layer_.get();
}

cc::Layer* LayerTestApi::cc_layer() {
  return layer_->cc_layer_;
}

const cc::Layer* LayerTestApi::cc_layer() const {
  return layer_->cc_layer_;
}

bool LayerTestApi::SwitchToSolidColorLayer() {
  if (!PrepareForLayerSwitch()) {
    return false;
  }

  auto* solid_color = layer_->AsSolidColor();
  CHECK(solid_color);
  scoped_refptr<cc::SolidColorLayer> new_layer = cc::SolidColorLayer::Create();
  solid_color->SwitchToLayer(new_layer);
  solid_color->solid_color_layer_ = std::move(new_layer);

  return true;
}

bool LayerTestApi::SwitchToTexturedLayer() {
  if (!PrepareForLayerSwitch()) {
    return false;
  }

  auto* textured = layer_->AsTextured();
  CHECK(textured);
  scoped_refptr<cc::PictureLayer> new_layer =
      cc::PictureLayer::Create(textured);
  textured->SwitchToLayer(new_layer);
  textured->content_layer_ = std::move(new_layer);

  return true;
}

const cc::Region& LayerTestApi::damaged_region() const {
  return layer_->damaged_region_;
}


bool LayerTestApi::IsPaintDeferred() const {
  CHECK(layer_->AsTextured());
  return layer_->AsTextured()->deferred_paint_requests_;
}

bool LayerTestApi::ContainsMirror(Layer* mirror) const {
  return std::ranges::contains(layer_->mirrors_, mirror,
                               &internal::LayerMirror::dest);
}

void LayerTestApi::SetCompositor(Compositor* compositor) {
  layer_->compositor_ = compositor;
}

bool LayerTestApi::PrepareForLayerSwitch() {
  Layer* layer = layer_;

  // Set layer_ to nullptr before calling FinishAnimationsBeforeSwitchToLayer
  // because that method can destroy the layer, leaving layer_ dangling.
  layer_ = nullptr;

  // If `FinishAnimationsBeforeSwitchToLayer` returns false, `layer` was
  // destroyed.
  if (!layer->FinishAnimationsBeforeSwitchToLayer()) {
    return false;
  }

  layer_ = layer;
  return true;
}

}  // namespace ui

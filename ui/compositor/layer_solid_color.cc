// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_solid_color.h"

#include <cstddef>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "cc/layers/mirror_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_mirror.h"

namespace ui {

LayerSolidColor::LayerSolidColor()
    : LayerWithExternalTexture(LAYER_SOLID_COLOR) {
  solid_color_layer_ = cc::SolidColorLayer::Create();
  cc_layer_ = solid_color_layer_.get();
  InitializeCcLayer();

  cc_layer_->SetSafeOpaqueBackgroundColor(SkColors::kBlack);
  cc_layer_->SetBackgroundColor(SkColors::kTransparent);

  // For LayerSolidColor, the background color dictates content opaqueness.
  cc_layer_->SetContentsOpaque(false);
  fills_bounds_opaquely_ = false;
}

LayerSolidColor::~LayerSolidColor() {
  Destroy();
}

std::unique_ptr<Layer> LayerSolidColor::Clone() const {
  auto clone = Layer::Clone();
  clone->AsSolidColor()->SetColor(GetTargetColor());
  return clone;
}

bool LayerSolidColor::ShouldSchedulePaint() const {
  // Only Schedule paint if LayerSolidColor has external content.
  return texture_layer() && LayerWithExternalTexture::ShouldSchedulePaint();
}

void LayerSolidColor::OnPaintScheduled() {
  ScheduleDraw();
}

void LayerSolidColor::SetShowReflectedLayerSubtree(
    Layer* subtree_reflected_layer) {
  DCHECK(subtree_reflected_layer);
  if (subtree_reflected_layer_ == subtree_reflected_layer) {
    return;
  }

  // If `FinishAnimationsBeforeSwitchToLayer` returns false, `this` Layer was
  // destroyed.
  if (!FinishAnimationsBeforeSwitchToLayer()) {
    return;
  }

  scoped_refptr<cc::MirrorLayer> new_layer =
      cc::MirrorLayer::Create(subtree_reflected_layer->cc_layer_.get());
  SwitchToLayer(new_layer);

  mirror_layer_ = std::move(new_layer);

  subtree_reflected_layer_ = subtree_reflected_layer;
  auto insert_pair =
      subtree_reflected_layer_->subtree_reflecting_layers_.insert(this);
  DCHECK(insert_pair.second);

  MatchLayerSize(subtree_reflected_layer_);

  RecomputeDrawsContentAndUVRect();
}

void LayerSolidColor::SetShowSolidColorContent() {
  if (solid_color_layer_.get()) {
    return;
  }

  // If `FinishAnimationsBeforeSwitchToLayer` returns false, `this` Layer was
  // destroyed.
  if (!FinishAnimationsBeforeSwitchToLayer()) {
    return;
  }

  scoped_refptr<cc::SolidColorLayer> new_layer = cc::SolidColorLayer::Create();
  SwitchToLayer(new_layer);

  solid_color_layer_ = new_layer;
  fills_bounds_opaquely_ = cc_layer_->background_color().isOpaque();

  RecomputeDrawsContentAndUVRect();
  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsSolidColor()->SetShowSolidColorContent();
  }
}

void LayerSolidColor::SetColor(SkColor4f color) {
  GetAnimator()->SetColor(color);
}

SkColor4f LayerSolidColor::GetTargetColor() const {
  if (animator_ &&
      animator_->IsAnimatingProperty(LayerAnimationElement::COLOR)) {
    return animator_->GetTargetColor();
  }
  return cc_layer_->background_color();
}

SkColor4f LayerSolidColor::background_color() const {
  return cc_layer_->background_color();
}

void LayerSolidColor::Reset() {
  LayerWithExternalTexture::Reset();
  ResetSubtreeReflectedLayer();
  solid_color_layer_ = nullptr;
  mirror_layer_ = nullptr;
}

void LayerSolidColor::SetColorFromAnimation(SkColor4f color,
                                            PropertyChangeReason reason) {
  // For LayerSolidColor, the background color dictates content opaqueness.
  // And `SetContentOpaque()` is called in
  // `SolidColorLayer::SetBackgroundColor()`.
  cc_layer_->SetBackgroundColor(color);
  cc_layer_->SetSafeOpaqueBackgroundColor(color);
  SetFillsBoundsOpaquelyWithReason(color.isOpaque(), reason);
}

SkColor4f LayerSolidColor::GetColorForAnimation() const {
  // The NULL check is here since the underlying solid_color_layer can be
  // swapped with mirror_layer or textured_layer. See calls to
  // `SwitchToLayer()`
  return solid_color_layer_.get() ? solid_color_layer_->background_color()
                                  : SkColors::kBlack;
}

void LayerSolidColor::ResetSubtreeReflectedLayer() {
  if (!subtree_reflected_layer_) {
    return;
  }

  size_t result =
      subtree_reflected_layer_->subtree_reflecting_layers_.erase(this);
  DCHECK_EQ(1u, result);
  subtree_reflected_layer_ = nullptr;
}

}  // namespace ui

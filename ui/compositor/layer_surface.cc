// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_surface.h"

#include <algorithm>
#include <utility>

#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "ui/compositor/layer_mirror.h"

namespace ui {

LayerSurface::LayerSurface() : Layer(LAYER_SURFACE) {
  surface_layer_ = cc::SurfaceLayer::Create();
  cc_layer_ = surface_layer_.get();

  InitializeCcLayer();
  surface_layer_->SetSurfaceHitTestable(true);
}

LayerSurface::~LayerSurface() {
  Destroy();
}

bool LayerSurface::HasExternalContent() const {
  return true;
}

bool LayerSurface::ShouldSchedulePaint() const {
  return false;
}

void LayerSurface::SetBackgroundColor(SkColor4f color) {
  surface_layer_->SetBackgroundColor(color);
  surface_layer_->SetSafeOpaqueBackgroundColor(color);

  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsSurface()->SetBackgroundColor(color);
  }
}

SkColor4f LayerSurface::GetBackgroundColor() const {
  return surface_layer_->background_color();
}

std::unique_ptr<Layer> LayerSurface::Clone() const {
  auto clone = Layer::Clone();
  auto* cloned_surface = clone->AsSurface();

  cloned_surface->SetBackgroundColor(surface_layer_->background_color());
  cloned_surface->SetShowSurface(
      surface_layer_->surface_id(), frame_size_in_dip_,
      surface_layer_->deadline_in_frames()
          ? cc::DeadlinePolicy::UseSpecifiedDeadline(
                *surface_layer_->deadline_in_frames())
          : cc::DeadlinePolicy::UseDefaultDeadline(),
      surface_layer_->stretch_content_to_fill_bounds());
  if (surface_layer_->oldest_acceptable_fallback()) {
    cloned_surface->SetOldestAcceptableFallback(
        *surface_layer_->oldest_acceptable_fallback());
  }

  return clone;
}

void LayerSurface::SetShowSurface(const viz::SurfaceId& surface_id,
                                  const gfx::Size& frame_size_in_dip,
                                  const cc::DeadlinePolicy& deadline_policy,
                                  bool stretch_content_to_fill_bounds) {
  surface_layer_->SetSurfaceId(surface_id, deadline_policy);
  surface_layer_->SetStretchContentToFillBounds(stretch_content_to_fill_bounds);

  frame_size_in_dip_ = frame_size_in_dip;
  RecomputeDrawsContentAndUVRect();

  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsSurface()->SetShowSurface(surface_id, frame_size_in_dip,
                                                deadline_policy,
                                                stretch_content_to_fill_bounds);
  }
}

void LayerSurface::SetShowSurface(const viz::SurfaceId& surface_id,
                                  const cc::DeadlinePolicy& deadline_policy,
                                  bool stretch_content_to_fill_bounds) {
  // Assumes `frame_size_in_dip_` is already set.
  // TODO(crbug.com/40285157): with surface sync, it should use on `bounds_`.
  surface_layer_->SetSurfaceId(surface_id, deadline_policy);
  surface_layer_->SetStretchContentToFillBounds(stretch_content_to_fill_bounds);

  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsSurface()->SetShowSurface(surface_id, deadline_policy,
                                                stretch_content_to_fill_bounds);
  }
}

void LayerSurface::SetShowReflectedSurface(
    const viz::SurfaceId& surface_id,
    const gfx::Size& frame_size_in_pixels) {
  surface_layer_->SetSurfaceId(surface_id,
                               cc::DeadlinePolicy::UseInfiniteDeadline());
  surface_layer_->SetBackgroundColor(SkColors::kBlack);
  surface_layer_->SetSafeOpaqueBackgroundColor(SkColors::kBlack);
  surface_layer_->SetStretchContentToFillBounds(true);
  surface_layer_->SetIsReflection(true);

  // The reflecting surface uses the native size of the reflected display.
  frame_size_in_dip_ = frame_size_in_pixels;
  RecomputeDrawsContentAndUVRect();
}

void LayerSurface::SetOldestAcceptableFallback(
    const viz::SurfaceId& surface_id) {
  surface_layer_->SetOldestAcceptableFallback(surface_id);

  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsSurface()->SetOldestAcceptableFallback(surface_id);
  }
}

const viz::SurfaceId* LayerSurface::GetOldestAcceptableFallback() const {
  if (surface_layer_->oldest_acceptable_fallback()) {
    return &surface_layer_->oldest_acceptable_fallback().value();
  }

  return nullptr;
}

bool LayerSurface::StretchContentToFillBounds() const {
  return surface_layer_->stretch_content_to_fill_bounds();
}

void LayerSurface::SetSurfaceSize(gfx::Size surface_size_in_dip) {
  if (frame_size_in_dip_ == surface_size_in_dip) {
    return;
  }

  frame_size_in_dip_ = surface_size_in_dip;
  RecomputeDrawsContentAndUVRect();
}

const viz::SurfaceId* LayerSurface::GetSurfaceId() const {
  if (surface_layer_->surface_id().is_valid()) {
    return &surface_layer_->surface_id();
  }

  return nullptr;
}

void LayerSurface::RecomputeDrawsContentAndUVRect() {
  gfx::Size size(bounds_.size());
  // TODO(crbug.com/40285157): with surface sync, size shouldn't rely on
  // `frame_size_in_dip_` anymore.
  size.SetToMin(frame_size_in_dip_);
  cc_layer_->SetBounds(size);
}

void LayerSurface::Reset() {
  surface_layer_ = nullptr;
}

}  // namespace ui

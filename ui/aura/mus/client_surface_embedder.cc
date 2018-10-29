// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/client_surface_embedder.h"

#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/geometry/dip_util.h"

namespace aura {

ClientSurfaceEmbedder::ClientSurfaceEmbedder(
    Window* window,
    bool inject_gutter,
    const gfx::Insets& client_area_insets)
    : window_(window),
      surface_layer_owner_(std::make_unique<ui::LayerOwner>(
          std::make_unique<ui::Layer>(ui::LAYER_TEXTURED))),
      inject_gutter_(inject_gutter),
      client_area_insets_(client_area_insets) {
  surface_layer_owner_->layer()->SetMasksToBounds(true);
  // The frame provided by the parent window->layer() needs to show through
  // the surface layer.
  surface_layer_owner_->layer()->SetFillsBoundsOpaquely(false);

  window_->layer()->Add(surface_layer_owner_->layer());

  // Window's layer may contain content from this client (the embedder), e.g.
  // this is the case with window decorations provided by Window Manager.
  // This content should appear underneath the content of the embedded client.
  window_->layer()->StackAtTop(surface_layer_owner_->layer());
}

ClientSurfaceEmbedder::~ClientSurfaceEmbedder() = default;

void ClientSurfaceEmbedder::SetSurfaceId(const viz::SurfaceId& surface_id) {
  surface_layer_owner_->layer()->SetShowSurface(
      surface_id, window_->bounds().size(), SK_ColorWHITE,
      cc::DeadlinePolicy::UseDefaultDeadline(),
      false /* stretch_content_to_fill_bounds */);
}

bool ClientSurfaceEmbedder::HasPrimarySurfaceId() const {
  return surface_layer_owner_->layer()->GetSurfaceId() != nullptr;
}

void ClientSurfaceEmbedder::SetFallbackSurfaceInfo(
    const viz::SurfaceInfo& surface_info) {
  fallback_surface_info_ = surface_info;
  surface_layer_owner_->layer()->SetOldestAcceptableFallback(surface_info.id());
  UpdateSizeAndGutters();
}

void ClientSurfaceEmbedder::SetClientAreaInsets(
    const gfx::Insets& client_area_insets) {
  if (client_area_insets_ == client_area_insets)
    return;

  client_area_insets_ = client_area_insets;
  if (inject_gutter_)
    UpdateSizeAndGutters();
}

void ClientSurfaceEmbedder::UpdateSizeAndGutters() {
  surface_layer_owner_->layer()->SetBounds(gfx::Rect(window_->bounds().size()));
  if (!inject_gutter_)
    return;

  gfx::Size fallback_surface_size_in_dip;
  if (fallback_surface_info_.is_valid()) {
    float fallback_device_scale_factor =
        fallback_surface_info_.device_scale_factor();
    fallback_surface_size_in_dip = gfx::ConvertSizeToDIP(
        fallback_device_scale_factor, fallback_surface_info_.size_in_pixels());
  }
  gfx::Rect window_bounds(window_->bounds());
  if (!window_->transparent() &&
      fallback_surface_size_in_dip.width() < window_bounds.width()) {
    right_gutter_owner_ = std::make_unique<ui::LayerOwner>(
        std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR));
    // TODO(fsamuel): Use the embedded client's background color.
    right_gutter_owner_->layer()->SetColor(SK_ColorWHITE);
    int width = window_bounds.width() - fallback_surface_size_in_dip.width();
    // The right gutter also includes the bottom-right corner, if necessary.
    int height = window_bounds.height() - client_area_insets_.height();
    right_gutter_owner_->layer()->SetBounds(gfx::Rect(
        client_area_insets_.left() + fallback_surface_size_in_dip.width(),
        client_area_insets_.top(), width, height));
    window_->layer()->Add(right_gutter_owner_->layer());
  } else {
    right_gutter_owner_.reset();
  }

  // Only create a bottom gutter if a fallback surface is available. Otherwise,
  // the right gutter will fill the whole window until a fallback is available.
  if (!window_->transparent() && !fallback_surface_size_in_dip.IsEmpty() &&
      fallback_surface_size_in_dip.height() < window_bounds.height()) {
    bottom_gutter_owner_ = std::make_unique<ui::LayerOwner>(
        std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR));
    // TODO(fsamuel): Use the embedded client's background color.
    bottom_gutter_owner_->layer()->SetColor(SK_ColorWHITE);
    int width = fallback_surface_size_in_dip.width();
    int height = window_bounds.height() - fallback_surface_size_in_dip.height();
    bottom_gutter_owner_->layer()->SetBounds(
        gfx::Rect(0, fallback_surface_size_in_dip.height(), width, height));
    window_->layer()->Add(bottom_gutter_owner_->layer());
  } else {
    bottom_gutter_owner_.reset();
  }
  window_->layer()->StackAtTop(surface_layer_owner_->layer());
}

ui::Layer* ClientSurfaceEmbedder::RightGutterForTesting() {
  return right_gutter_owner_ ? right_gutter_owner_->layer() : nullptr;
}

ui::Layer* ClientSurfaceEmbedder::BottomGutterForTesting() {
  return bottom_gutter_owner_ ? bottom_gutter_owner_->layer() : nullptr;
}

const viz::SurfaceId& ClientSurfaceEmbedder::GetSurfaceIdForTesting() const {
  return *surface_layer_owner_->layer()->GetSurfaceId();
}

}  // namespace aura

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_with_external_texture.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "cc/layers/texture_layer.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/compositor/layer_mirror.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

LayerWithExternalTexture::~LayerWithExternalTexture() = default;

void LayerWithExternalTexture::SetTransferableResource(
    const viz::TransferableResource& resource,
    viz::ReleaseCallback release_callback,
    gfx::Size texture_size_in_dip) {
  DCHECK(!resource.is_empty());
  DCHECK(release_callback);
  DCHECK(!resource.GetIsSoftware());
  if (!texture_layer_.get()) {
    // If `FinishAnimationsBeforeSwitchToLayer` returns false, `this` Layer was
    // destroyed.
    if (!FinishAnimationsBeforeSwitchToLayer()) {
      return;
    }
    // Incoming resource is assumed to have top-left origin which corresponds to
    // TextureLayer flipped being false.
    scoped_refptr<cc::TextureLayer> new_layer = cc::TextureLayer::Create(this);
    SwitchToLayer(new_layer);

    texture_layer_ = new_layer;
    // Reset the texture_size_in_dip_ so that SetTextureSize() will not early
    // out, the texture_size_in_dip_ was for a previous (different)
    // |texture_layer_|.
    texture_size_in_dip_ = gfx::Size();
  }

  if (transfer_release_callback_) {
    std::move(transfer_release_callback_).Run(gpu::SyncToken(), false);
  }

  transfer_release_callback_ = std::move(release_callback);
  transfer_resource_ = resource;
  SetTextureSize(texture_size_in_dip);

  for (const auto& mirror : mirrors_) {
    // The release callbacks should be empty as only the source layer
    // should be able to release the texture resource.
    static_cast<LayerWithExternalTexture*>(mirror->dest())
        ->SetTransferableResource(
            transfer_resource_,
            base::BindOnce(
                [](const gpu::SyncToken& sync_token, bool is_lost) {}),
            texture_size_in_dip_);
  }
}

void LayerWithExternalTexture::SetTextureSize(gfx::Size texture_size_in_dip) {
  if (texture_size_in_dip_ == texture_size_in_dip) {
    return;
  }

  texture_size_in_dip_ = texture_size_in_dip;
  RecomputeDrawsContentAndUVRect();
  texture_layer_->SetNeedsDisplay();
}

bool LayerWithExternalTexture::HasTransferableResource() const {
  return !transfer_resource_.is_empty();
}

std::unique_ptr<Layer> LayerWithExternalTexture::CreateMirror(
    const LayerMirrorSettings& settings) {
  auto mirror = Layer::CreateMirror(settings);

  if (HasTransferableResource()) {
    // Send an empty release callback because we don't want the resource to be
    // freed up until the original layer releases it.
    static_cast<LayerWithExternalTexture*>(mirror.get())
        ->SetTransferableResource(
            transfer_resource(),
            base::BindOnce(
                [](const gpu::SyncToken& sync_token, bool is_lost) {}),
            texture_size_in_dip_);
  }

  return mirror;
}

bool LayerWithExternalTexture::HasExternalContent() const {
  return texture_layer_.get();
}

void LayerWithExternalTexture::RecomputeDrawsContentAndUVRect() {
  gfx::Size size(bounds_.size());
  if (texture_layer_.get()) {
    size.SetToMin(texture_size_in_dip_);
    gfx::PointF uv_top_left(0.f, 0.f);
    gfx::PointF uv_bottom_right(
        static_cast<float>(size.width()) / texture_size_in_dip_.width(),
        static_cast<float>(size.height()) / texture_size_in_dip_.height());
    texture_layer_->SetUV(uv_top_left, uv_bottom_right);
  }

  cc_layer_->SetBounds(size);
}

bool LayerWithExternalTexture::ShouldSchedulePaint() const {
  // A layer with an external texture needs to schedule a paint when it has a
  // transferable resource. Even though it doesn't use a delegate to paint
  // contents, scheduling a paint is necessary to accumulate damage and trigger
  // a frame draw in the compositor to display the updated texture.
  return HasTransferableResource();
}

void LayerWithExternalTexture::OnPaintScheduled() {
  ScheduleDraw();
}

bool LayerWithExternalTexture::ShouldCommitDamage() const {
  // A layer with an external texture needs to commit damage when it has a
  // transferable resource to display.
  return HasTransferableResource();
}

bool LayerWithExternalTexture::PrepareTransferableResource(
    viz::TransferableResource* resource,
    viz::ReleaseCallback* release_callback) {
  if (!transfer_release_callback_) {
    return false;
  }

  *resource = transfer_resource_;
  *release_callback = std::move(transfer_release_callback_);
  return true;
}

LayerWithExternalTexture::LayerWithExternalTexture(LayerType type)
    : Layer(type) {}

void LayerWithExternalTexture::Reset() {
  if (texture_layer_.get()) {
    texture_layer_->ClearClient();
  }

  texture_layer_ = nullptr;
  transfer_resource_ = viz::TransferableResource();
  if (transfer_release_callback_) {
    std::move(transfer_release_callback_).Run(gpu::SyncToken(), false);
  }
}

}  // namespace ui

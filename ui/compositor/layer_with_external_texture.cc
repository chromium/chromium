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

LayerWithExternalTexture::LayerWithExternalTexture()
    : Layer(LAYER_WITH_EXTERNAL_TEXTURE) {
  texture_layer_ = cc::TextureLayer::Create(this);
  cc_layer_ = texture_layer_.get();
  InitializeCcLayer();
}

LayerWithExternalTexture::~LayerWithExternalTexture() {
  Destroy();
}

void LayerWithExternalTexture::SetTransferableResource(
    const viz::TransferableResource& resource,
    viz::ReleaseCallback release_callback,
    gfx::Size texture_size_in_dip) {
  DCHECK(!resource.is_empty());
  DCHECK(release_callback);
  DCHECK(!resource.GetIsSoftware());

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

void LayerWithExternalTexture::ClearTexture() {
  if (!HasTransferableResource()) {
    return;
  }

  texture_layer_->ClearTexture();
  texture_layer_->SetNeedsSetTransferableResource();

  transfer_resource_ = viz::TransferableResource();
  if (transfer_release_callback_) {
    std::move(transfer_release_callback_).Run(gpu::SyncToken(), false);
  }

  for (const auto& mirror : mirrors_) {
    mirror->dest()->AsWithExternalTexture()->ClearTexture();
  }
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
  CHECK(HasTransferableResource());
  ScheduleDraw();
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_WITH_EXTERNAL_TEXTURE_H_
#define UI_COMPOSITOR_LAYER_WITH_EXTERNAL_TEXTURE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "cc/layers/texture_layer_client.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class TextureLayer;
}  // namespace cc

namespace ui {

// Base class for layers that can display an externally supplied GPU-backed
// transferable resource (backed by a cc::TextureLayer).
class COMPOSITOR_EXPORT LayerWithExternalTexture
    : public Layer,
      public cc::TextureLayerClient {
 public:
  LayerWithExternalTexture(const LayerWithExternalTexture&) = delete;
  LayerWithExternalTexture& operator=(const LayerWithExternalTexture&) = delete;

  ~LayerWithExternalTexture() override;

  // Set new TransferableResource for this layer. This method only supports
  // a gpu-backed `resource` which is assumed to have top-left origin.
  void SetTransferableResource(const viz::TransferableResource& resource,
                               viz::ReleaseCallback release_callback,
                               gfx::Size texture_size_in_dip);
  void SetTextureSize(gfx::Size texture_size_in_dip);

  bool HasTransferableResource() const;

  // Layer:
  bool HasExternalContent() const override;
  void RecomputeDrawsContentAndUVRect() override;
  bool ShouldSchedulePaint() const override;

  // TextureLayerClient:
  bool PrepareTransferableResource(
      viz::TransferableResource* resource,
      viz::ReleaseCallback* release_callback) override;

  const viz::TransferableResource& transfer_resource() const {
    return transfer_resource_;
  }

 protected:
  explicit LayerWithExternalTexture(LayerType type);

  // Layer:
  std::unique_ptr<Layer> CreateMirror(
      const LayerMirrorSettings& settings) override;
  void Reset() override;
  void OnPaintScheduled() override;
  bool ShouldCommitDamage() const override;

  cc::TextureLayer* texture_layer() { return texture_layer_.get(); }
  const cc::TextureLayer* texture_layer() const { return texture_layer_.get(); }

 private:
  gfx::Size texture_size_in_dip_;
  scoped_refptr<cc::TextureLayer> texture_layer_;
  viz::TransferableResource transfer_resource_;
  viz::ReleaseCallback transfer_release_callback_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_WITH_EXTERNAL_TEXTURE_H_

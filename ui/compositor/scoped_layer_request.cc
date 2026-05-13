
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/scoped_layer_request.h"

#include "base/check.h"
#include "ui/compositor/layer.h"

namespace ui {

////////////////////////////////////////////////////////////////////////////////
// ScopedLayerRequest<LayerRequestType type>

template <LayerRequestType type>
Layer* ScopedLayerRequest<type>::GetLayer() const {
  return const_cast<Layer*>(observation_.GetSource());
}

template <LayerRequestType type>
void ScopedLayerRequest<type>::LayerDestroyed(Layer* layer) {
  observation_.Reset();
}

////////////////////////////////////////////////////////////////////////////////
// ScopedPaintLock

template <>
ScopedLayerRequest<LayerRequestType::kPaint>::ScopedLayerRequest(Layer* layer) {
  CHECK(layer);
  observation_.Observe(layer);
  layer->AddDeferredPaintRequest();
}
template <>
ScopedLayerRequest<LayerRequestType::kPaint>::~ScopedLayerRequest() {
  if (Layer* layer = GetLayer()) {
    layer->RemoveDeferredPaintRequest();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ScopedCacheRenderSurfaceLock

template <>
ScopedLayerRequest<LayerRequestType::kCacheRenderSurface>::ScopedLayerRequest(
    Layer* layer) {
  CHECK(layer);
  observation_.Observe(layer);
  layer->AddCacheRenderSurfaceRequest();
}
template <>
ScopedLayerRequest<
    LayerRequestType::kCacheRenderSurface>::~ScopedLayerRequest() {
  if (Layer* layer = GetLayer()) {
    layer->RemoveCacheRenderSurfaceRequest();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ScopedTrilinearFilteringLock

template <>
ScopedLayerRequest<LayerRequestType::kTrilinearFiltering>::ScopedLayerRequest(
    Layer* layer) {
  CHECK(layer);
  observation_.Observe(layer);
  layer->AddTrilinearFilteringRequest();
}

template <>
ScopedLayerRequest<
    LayerRequestType::kTrilinearFiltering>::~ScopedLayerRequest() {
  if (Layer* layer = GetLayer()) {
    layer->RemoveTrilinearFilteringRequest();
  }
}

template class COMPOSITOR_EXPORT ScopedLayerRequest<LayerRequestType::kPaint>;
template class COMPOSITOR_EXPORT
    ScopedLayerRequest<LayerRequestType::kCacheRenderSurface>;
template class COMPOSITOR_EXPORT
    ScopedLayerRequest<LayerRequestType::kTrilinearFiltering>;

}  // namespace ui

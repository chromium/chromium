
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_SCOPED_LAYER_REQUEST_H_
#define UI_COMPOSITOR_SCOPED_LAYER_REQUEST_H_

#include "base/scoped_observation.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_observer.h"

namespace ui {

template <LayerRequestType type>
class COMPOSITOR_EXPORT ScopedLayerRequest : public LayerObserver {
 public:
  explicit ScopedLayerRequest(Layer* layer);

  ScopedLayerRequest(const ScopedLayerRequest&) = delete;
  ScopedLayerRequest& operator=(const ScopedLayerRequest&) = delete;

  ~ScopedLayerRequest() override;

  Layer* GetLayer() const;

 private:
  // LayerObserver:
  void LayerDestroyed(Layer* layer) override;
  base::ScopedObservation<Layer, LayerObserver> observation_{this};
};

// A scoped object that defers painting on a given layer.
using ScopedPaintLock = ScopedLayerRequest<LayerRequestType::kPaint>;

// A scoped object that requests a cache render surface on a given layer.
using ScopedCacheRenderSurfaceLock =
    ScopedLayerRequest<LayerRequestType::kCacheRenderSurface>;

// A scoped object that requests trilinear filtering on a given layer.
using ScopedTrilinearFilteringLock =
    ScopedLayerRequest<LayerRequestType::kTrilinearFiltering>;

}  // namespace ui

#endif  // UI_COMPOSITOR_SCOPED_LAYER_REQUEST_H_

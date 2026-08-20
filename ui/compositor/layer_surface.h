// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_SURFACE_H_
#define UI_COMPOSITOR_LAYER_SURFACE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "cc/layers/deadline_policy.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class SurfaceLayer;
}  // namespace cc

namespace viz {
class SurfaceId;
}  // namespace viz

namespace ui {

// A layer that embeds and displays a viz::SurfaceId (e.g. from another
// process or display compositor), backed by a cc::SurfaceLayer.
class COMPOSITOR_EXPORT LayerSurface : public Layer {
 public:
  static constexpr LayerType kType = LAYER_SURFACE;

  LayerSurface();

  LayerSurface(const LayerSurface&) = delete;
  LayerSurface& operator=(const LayerSurface&) = delete;

  ~LayerSurface() override;

  // Layer:
  bool HasExternalContent() const override;
  std::unique_ptr<Layer> Clone() const override;
  bool ShouldSchedulePaint() const override;

  void SetBackgroundColor(SkColor4f color);
  SkColor4f GetBackgroundColor() const;

  // Begins showing content from a surface with a particular ID.
  // TODO(crbug.com/40285157): With surface sync, size shouldn't rely on
  // `frame_size_in_dip` anymore, so this method can be deleted, and
  // surface_size uses `bounds_` instead.
  void SetShowSurface(const viz::SurfaceId& surface_id,
                      const gfx::Size& frame_size_in_dip,
                      const cc::DeadlinePolicy& deadline_policy,
                      bool stretch_content_to_fill_bounds);

  // Updates the surface to a particular ID without changing size.
  void SetShowSurface(const viz::SurfaceId& surface_id,
                      const cc::DeadlinePolicy& deadline_policy,
                      bool stretch_content_to_fill_bounds);

  // Begins mirroring content from a reflected surface, e.g. a software mirrored
  // display. |surface_id| should be the root surface for a display.
  void SetShowReflectedSurface(const viz::SurfaceId& surface_id,
                               const gfx::Size& frame_size_in_pixels);

  // In the event that the primary surface is not yet available in the
  // display compositor, the fallback surface will be used.
  void SetOldestAcceptableFallback(const viz::SurfaceId& surface_id);

  // Returns the fallback SurfaceId set by SetOldestAcceptableFallback.
  const viz::SurfaceId* GetOldestAcceptableFallback() const;

  // If |surface_layer_| exists, return whether the contents should stretch to
  // fill the bounds of |this|. Defaults to false.
  bool StretchContentToFillBounds() const;

  // If |surface_layer_| exists, update the size. The updated size is necessary
  // for proper scaling if the embedder is resized and the |surface_layer_| is
  // set to stretch to fill bounds.
  void SetSurfaceSize(gfx::Size surface_size_in_dip);

  // Returns the primary SurfaceId set by SetShowSurface.
  const viz::SurfaceId* GetSurfaceId() const;

 protected:
  // Layer:
  void RecomputeDrawsContentAndUVRect() override;
  void Reset() override;

 private:
  gfx::Size frame_size_in_dip_;
  scoped_refptr<cc::SurfaceLayer> surface_layer_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_SURFACE_H_

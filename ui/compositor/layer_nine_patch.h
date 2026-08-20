// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_NINE_PATCH_H_
#define UI_COMPOSITOR_LAYER_NINE_PATCH_H_

#include "base/memory/scoped_refptr.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"

namespace cc {
class NinePatchLayer;
}  // namespace cc

namespace ui {

// A layer that displays a resizable 9-patch bitmap with scalable borders
// and center aperture, backed by a cc::NinePatchLayer.
class COMPOSITOR_EXPORT LayerNinePatch : public Layer {
 public:
  static constexpr LayerType kType = LAYER_NINE_PATCH;

  LayerNinePatch();

  LayerNinePatch(const LayerNinePatch&) = delete;
  LayerNinePatch& operator=(const LayerNinePatch&) = delete;

  ~LayerNinePatch() override;

  const gfx::Rect& border() const;

  const gfx::Rect& aperture() const;

  const gfx::Rect& occlusion() const;

  // Layer:
  bool ShouldSchedulePaint() const override;

  // Updates the nine patch layer's image, aperture and border.
  void UpdateNinePatchLayerImage(const gfx::ImageSkia& image);
  void UpdateNinePatchLayerAperture(const gfx::Rect& aperture_in_dip);
  void UpdateNinePatchLayerBorder(const gfx::Rect& border);
  // Updates the area completely occluded by another layer, this can be an
  // empty rectangle if nothing is occluded.
  void UpdateNinePatchOcclusion(const gfx::Rect& occlusion);

 private:
  // Layer:
  void HandleDeviceScaleFactorChange() override;
  void Reset() override;

  // A cached copy of the nine patch layer's image and aperture.
  // These are required for device scale factor change.
  gfx::ImageSkia nine_patch_layer_image_;
  gfx::Rect nine_patch_layer_aperture_;

  scoped_refptr<cc::NinePatchLayer> nine_patch_layer_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_NINE_PATCH_H_

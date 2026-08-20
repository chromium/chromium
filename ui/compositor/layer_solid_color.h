// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_SOLID_COLOR_H_
#define UI_COMPOSITOR_LAYER_SOLID_COLOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/layer_with_external_texture.h"

namespace cc {
class MirrorLayer;
class SolidColorLayer;
}  // namespace cc

namespace ui {

class LayerTestApi;

// A layer that renders a uniform solid color (backed by cc::SolidColorLayer),
// or mirrors a reflected subtree (via cc::MirrorLayer), or displays an
// external transferable texture.
class COMPOSITOR_EXPORT LayerSolidColor : public LayerWithExternalTexture {
 public:
  static constexpr LayerType kType = LAYER_SOLID_COLOR;

  LayerSolidColor();

  LayerSolidColor(const LayerSolidColor&) = delete;
  LayerSolidColor& operator=(const LayerSolidColor&) = delete;

  ~LayerSolidColor() override;

  // Sets up this layer to mirror output of |subtree_reflected_layer|, including
  // its entire hierarchy. |this| should not be a descendant of
  // |subtree_reflected_layer|. This is achieved by using
  // cc::MirrorLayer which forces a render surface for |subtree_reflected_layer|
  // to be able to embed it. This might cause extra GPU memory bandwidth and/or
  // read/writes which can impact performance negatively.
  void SetShowReflectedLayerSubtree(Layer* subtree_reflected_layer);

  // Show a solid color instead of delegated or surface contents.
  void SetShowSolidColorContent();

  // Sets the layer's fill color.
  void SetColor(SkColor4f color);
  SkColor4f GetTargetColor() const;
  SkColor4f background_color() const;

  // Layer:
  std::unique_ptr<Layer> Clone() const override;
  bool ShouldSchedulePaint() const override;

 private:
  friend class LayerTestApi;

  // Layer:
  void Reset() override;
  void OnPaintScheduled() override;

  // LayerAnimatorDelegate:
  void SetColorFromAnimation(SkColor4f color,
                             PropertyChangeReason reason) override;
  SkColor4f GetColorForAnimation() const override;

  // Resets |subtree_reflected_layer_| and updates the reflected layer's
  // |subtree_reflecting_layers_| list accordingly.
  void ResetSubtreeReflectedLayer();

  // The layer being reflected with its subtree by this one, if any.
  raw_ptr<Layer> subtree_reflected_layer_ = nullptr;

  scoped_refptr<cc::SolidColorLayer> solid_color_layer_;
  scoped_refptr<cc::MirrorLayer> mirror_layer_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_SOLID_COLOR_H_

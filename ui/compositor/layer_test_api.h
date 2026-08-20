// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_TEST_API_H_
#define UI_COMPOSITOR_LAYER_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class Layer;
class MirrorLayer;
class Region;
}  // namespace cc

namespace ui {

class Compositor;
class Layer;

class COMPOSITOR_EXPORT LayerTestApi {
 public:
  explicit LayerTestApi(Layer* layer);

  LayerTestApi(const LayerTestApi&) = default;
  LayerTestApi& operator=(const LayerTestApi&) = default;

  ~LayerTestApi();

  // Returns the mirror layer.
  // Note: Only valid for LAYER_SOLID_COLOR.
  cc::MirrorLayer* mirror_layer();
  const cc::MirrorLayer* mirror_layer() const;

  // Returns the underlying cc::Layer.
  cc::Layer* cc_layer();
  const cc::Layer* cc_layer() const;

  // Swaps out the current cc::Layer with a new cc::SolidColorLayer. Returns
  // false if the layer was destroyed.
  // Note: Only valid for LAYER_SOLID_COLOR.
  bool SwitchToSolidColorLayer();

  // Swaps out the current cc::Layer with a new cc::PictureLayer. Returns false
  // if the layer was destroyed.
  // Note: Only valid for LAYER_TEXTURED.
  bool SwitchToTexturedLayer();

  // Returns the damaged region.
  const cc::Region& damaged_region() const;


  // Returns true if paint is currently deferred.
  // Note: Only valid for LAYER_TEXTURED.
  bool IsPaintDeferred() const;

  // Returns true if the given layer is a mirror of this layer.
  bool ContainsMirror(Layer* mirror) const;

  // Sets the compositor for this layer.
  void SetCompositor(Compositor* compositor);

 private:
  bool PrepareForLayerSwitch();

  raw_ptr<Layer> layer_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_TEST_API_H_

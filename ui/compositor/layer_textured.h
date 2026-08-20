// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_TEXTURED_H_
#define UI_COMPOSITOR_LAYER_TEXTURED_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "cc/base/region.h"
#include "cc/layers/content_layer_client.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/layer_with_external_texture.h"

namespace cc {
class DisplayItemList;
class PictureLayer;
}  // namespace cc

namespace ui {

class LayerTestApi;

// A layer whose contents are painted into a display item list via a
// LayerDelegate (backed by a cc::PictureLayer), or displays an external
// transferable texture.
class COMPOSITOR_EXPORT LayerTextured : public LayerWithExternalTexture,
                                        public cc::ContentLayerClient {
 public:
  static constexpr LayerType kType = LAYER_TEXTURED;

  LayerTextured();

  LayerTextured(const LayerTextured&) = delete;
  LayerTextured& operator=(const LayerTextured&) = delete;

  ~LayerTextured() override;

  // Requests deferring painting for this layer.
  // Note: While painting is deferred, damaged is accumulated, but it is not
  // committed to the cc::Layer (and no draw is scheduled at the compositor).
  // Once all deferred paint requests are removed, the accumulated damage is
  // committed and a draw is scheduled.
  void AddDeferredPaintRequest();
  void RemoveDeferredPaintRequest();

  // Layer:
  std::unique_ptr<Layer> Clone() const override;
  bool ShouldSchedulePaint() const override;

  // ContentLayerClient implementation.
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList() override;
  bool FillsBoundsCompletely() const override;

  // Set to true if this layer always paints completely within its bounds. If so
  // we can omit an unnecessary clear, even if the layer is transparent.
  void SetFillsBoundsCompletely(bool fills_bounds_completely);

  cc::PictureLayer* content_layer() { return content_layer_.get(); }

 protected:
  // Layer:
  void OnPaintScheduled() override;
  bool ShouldCommitDamage() const override;
  void CommitDamage(const cc::Region& damage) override;
  void Reset() override;

 private:
  friend class LayerTestApi;

  // See `SetFillsBoundsCompletely()`.
  bool fills_bounds_completely_ = false;

  // Union of damaged rects, in layer space, to be used when compositor is ready
  // to paint the content.
  cc::Region paint_region_;

  // The counter to maintain how many deferred paint requests we have. If the
  // value > 0, means we need to defer painting the layer. If the value == 0,
  // means we should paint the layer.
  unsigned deferred_paint_requests_ = 0u;

  scoped_refptr<cc::PictureLayer> content_layer_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_TEXTURED_H_

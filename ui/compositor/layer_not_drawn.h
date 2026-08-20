// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_NOT_DRAWN_H_
#define UI_COMPOSITOR_LAYER_NOT_DRAWN_H_

#include "base/memory/scoped_refptr.h"
#include "cc/layers/content_layer_client.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"

namespace cc {
class DisplayItemList;
class PictureLayer;
}  // namespace cc

namespace ui {

// A layer that has no onscreen representation of its own, but serves as a
// container in the layer hierarchy whose children are drawn.
class COMPOSITOR_EXPORT LayerNotDrawn : public Layer,
                                        public cc::ContentLayerClient {
 public:
  static constexpr LayerType kType = LAYER_NOT_DRAWN;

  LayerNotDrawn();

  LayerNotDrawn(const LayerNotDrawn&) = delete;
  LayerNotDrawn& operator=(const LayerNotDrawn&) = delete;

  ~LayerNotDrawn() override;

  // Layer:
  bool ShouldSchedulePaint() const override;

  // ContentLayerClient implementation.
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList() override;
  bool FillsBoundsCompletely() const override;

 private:
  // Layer:
  void Reset() override;

  scoped_refptr<cc::PictureLayer> content_layer_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_NOT_DRAWN_H_

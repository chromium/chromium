// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_MIRROR_H_
#define UI_COMPOSITOR_LAYER_MIRROR_H_

#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_observer.h"

namespace ui {

class Layer;
class PaintContext;

namespace internal {

// Manages the mirroring between a source and a destination layer.
class LayerMirror : public LayerDelegate, public LayerObserver {
 public:
  LayerMirror(Layer* source, Layer* dest);

  LayerMirror(const LayerMirror&) = delete;
  LayerMirror& operator=(const LayerMirror&) = delete;

  ~LayerMirror() override;

  Layer* dest() { return dest_; }

  // LayerDelegate:
  void OnPaintLayer(const PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // LayerObserver:
  void LayerDestroyed(Layer* layer) override;

 private:
  const raw_ptr<Layer> source_;
  const raw_ptr<Layer> dest_;
};

}  // namespace internal
}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_MIRROR_H_

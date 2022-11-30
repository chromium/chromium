// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_DISPLAY_CA_LAYER_TREE_H_
#define UI_ACCELERATED_WIDGET_MAC_DISPLAY_CA_LAYER_TREE_H_

#include <IOSurface/IOSurface.h>

#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/accelerated_widget_mac/ca_layer_frame_sink.h"

@class CALayer;
@class CALayerHost;

namespace gfx {
struct CALayerParams;
class Size;
}  // namespace gfx

namespace ui {

// Used to create a CALayer tree for displaying compositor output. This is
// created with a CALayer (usually the layer of a layer-hosting NSView) to use
// as the root layer of a small layer tree that displays contents specified via
// CALayerParams from a compositor's output.
class ACCELERATED_WIDGET_MAC_EXPORT DisplayCALayerTree
    : public CALayerFrameSink {
 public:
  explicit DisplayCALayerTree(CALayer* root_layer);
  ~DisplayCALayerTree() override;

  void UpdateCALayerTree(const gfx::CALayerParams& ca_layer_params) override;

 private:
  void GotCALayerFrame(uint32_t ca_context_id);
  void GotIOSurfaceFrame(base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
                         const gfx::Size& pixel_size,
                         float scale_factor);

  // The root layer of the tree specified at creation time.
  base::scoped_nsobject<CALayer> root_layer_;

  // A flipped layer, which acts as the parent of either |remote_layer_| or
  // |io_surface_layer|. This layer is flipped so that the we don't need to
  // recompute the origin for sub-layers when their position changes (this is
  // impossible when using remote layers, as their size change cannot be
  // synchronized with the window). This indirection is needed because flipping
  // hosted layers (like |background_layer_|) leads to unpredictable behavior.
  base::scoped_nsobject<CALayer> flipped_layer_;

  // A remote CALayer with content provided by the output surface.
  base::scoped_nsobject<CALayerHost> remote_layer_;

  // A CALayer that has its content set to an IOSurface.
  base::scoped_nsobject<CALayer> io_surface_layer_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_DISPLAY_CA_LAYER_TREE_H_

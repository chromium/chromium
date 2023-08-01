// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_LAYER_TREE_COORDINATOR_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_LAYER_TREE_COORDINATOR_H_

#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/accelerated_widget_mac/ca_renderer_layer_tree.h"

namespace ui {

// A structure that holds the tree of CALayers to display composited content.
// The CALayer tree may consist of a GLRendererLayerTree if the OpenGL renderer
// is being used, or a CARendererLayerTree if the CoreAnimation renderer is
// being used.
//
// This is instantiated in the GPU process and sent to the browser process via
// the cross-process CoreAnimation API.
class ACCELERATED_WIDGET_MAC_EXPORT CALayerTreeCoordinator {
 public:
  explicit CALayerTreeCoordinator(bool allow_remote_layers,
                                  bool allow_av_sample_buffer_display_layer);

  CALayerTreeCoordinator(const CALayerTreeCoordinator&) = delete;
  CALayerTreeCoordinator& operator=(const CALayerTreeCoordinator&) = delete;

  ~CALayerTreeCoordinator();

  // Set the composited frame's size.
  void Resize(const gfx::Size& pixel_size, float scale_factor);

  // The CARendererLayerTree for the pending frame. This is used to construct
  // the CALayer tree for the CoreAnimation renderer.
  CARendererLayerTree* GetPendingCARendererLayerTree();
  bool HasPendingCARendererLayerTree() const {
    return !!pending_ca_renderer_layer_tree_;
  }

  // Commit the pending frame's OpenGL backbuffer or CALayer tree to be
  // attached to the root CALayer.
  void CommitPendingTreesToCA();

  // Get the root CALayer to display the current frame. This does not change
  // over the lifetime of the object.
  CALayer* GetCALayerForDisplay() const;

  // Get the current frame's OpenGL backbuffer IOSurface. This is only needed
  // when not using remote layers.
  IOSurfaceRef GetIOSurfaceForDisplay();

 private:
  const bool allow_remote_layers_ = true;
  const bool allow_av_sample_buffer_display_layer_ = true;
  gfx::Size pixel_size_;
  float scale_factor_ = 1;

  CALayer* __strong root_ca_layer_;

  // Frame that has been scheduled, but has not had a subsequent commit call
  // made yet.
  std::unique_ptr<CARendererLayerTree> pending_ca_renderer_layer_tree_;

  // Frame that is currently being displayed on the screen.
  std::unique_ptr<CARendererLayerTree> current_ca_renderer_layer_tree_;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_LAYER_TREE_COORDINATOR_H_

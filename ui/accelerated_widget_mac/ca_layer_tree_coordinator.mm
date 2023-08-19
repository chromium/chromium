// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"

#import <AVFoundation/AVFoundation.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/cocoa/animation_utils.h"

namespace ui {

CALayerTreeCoordinator::CALayerTreeCoordinator(
    bool allow_remote_layers,
    bool allow_av_sample_buffer_display_layer)
    : allow_remote_layers_(allow_remote_layers),
      allow_av_sample_buffer_display_layer_(
          allow_av_sample_buffer_display_layer) {
  if (allow_remote_layers_) {
    root_ca_layer_ = [[CALayer alloc] init];
#if BUILDFLAG(IS_MAC)
    // iOS' UIKit has default coordinate system where the origin is at the upper
    // left of the drawing area. In contrast, AppKit and Core Graphics that
    // macOS uses has its origin at the lower left of the drawing area. Thus, we
    // don't need to flip the coordinate system on iOS as it's already set the
    // way we want it to be.
    root_ca_layer_.geometryFlipped = YES;
#endif
    root_ca_layer_.opaque = YES;
  }
}

CALayerTreeCoordinator::~CALayerTreeCoordinator() = default;

void CALayerTreeCoordinator::Resize(const gfx::Size& pixel_size,
                                    float scale_factor) {
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
}

CARendererLayerTree* CALayerTreeCoordinator::GetPendingCARendererLayerTree() {
  if (!pending_ca_renderer_layer_tree_)
    pending_ca_renderer_layer_tree_ = std::make_unique<CARendererLayerTree>(
        allow_av_sample_buffer_display_layer_, false);
  return pending_ca_renderer_layer_tree_.get();
}

void CALayerTreeCoordinator::CommitPendingTreesToCA() {
  // Update the CALayer hierarchy.
  ScopedCAActionDisabler disabler;
  if (pending_ca_renderer_layer_tree_) {
    pending_ca_renderer_layer_tree_->CommitScheduledCALayers(
        root_ca_layer_, std::move(current_ca_renderer_layer_tree_), pixel_size_,
        scale_factor_);
    current_ca_renderer_layer_tree_.swap(pending_ca_renderer_layer_tree_);
  } else {
    TRACE_EVENT0("gpu", "Blank frame: No overlays or CALayers");
    DLOG(WARNING) << "Blank frame: No overlays or CALayers";
    root_ca_layer_.sublayers = nil;
    current_ca_renderer_layer_tree_.reset();
  }

  // Reset all state for the next frame.
  pending_ca_renderer_layer_tree_.reset();
}

CALayer* CALayerTreeCoordinator::GetCALayerForDisplay() const {
  DCHECK(allow_remote_layers_);
  return root_ca_layer_;
}

IOSurfaceRef CALayerTreeCoordinator::GetIOSurfaceForDisplay() {
  DCHECK(!allow_remote_layers_);
  if (!current_ca_renderer_layer_tree_)
    return nullptr;
  return current_ca_renderer_layer_tree_->GetContentIOSurface();
}

}  // namespace ui

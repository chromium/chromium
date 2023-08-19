// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_LAYER_FRAME_SINK_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_LAYER_FRAME_SINK_H_

#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/gfx/ca_layer_params.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// An interface to an NSView that will embed content described by CALayerParams
// in its hierarchy.
class ACCELERATED_WIDGET_MAC_EXPORT CALayerFrameSink {
 public:
  virtual ~CALayerFrameSink() = default;

  // Translate from a gfx::AcceleratedWidget to the gfx::CALayerFrameSink
  // interface through which frames may be submitted. This may return nullptr.
  static CALayerFrameSink* FromAcceleratedWidget(gfx::AcceleratedWidget widget);

  // Update the embedder's CALayer tree to show the content described by
  // |ca_layer_params|.
  virtual void UpdateCALayerTree(const gfx::CALayerParams& ca_layer_params) = 0;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_LAYER_FRAME_SINK_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_layer_frame_sink.h"

#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"

namespace ui {

// static
CALayerFrameSink* CALayerFrameSink::FromAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  return AcceleratedWidgetMac::Get(widget);
}

}  // namespace ui

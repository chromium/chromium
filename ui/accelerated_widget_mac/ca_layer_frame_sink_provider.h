// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_LAYER_FRAME_SINK_PROVIDER_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_LAYER_FRAME_SINK_PROVIDER_H_

#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/gfx/ca_layer_params.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class CALayerFrameSink;
}

@interface CALayerFrameSinkProvider : UIView
- (ui::CALayerFrameSink*)frameSink;

@end

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_LAYER_FRAME_SINK_PROVIDER_H_

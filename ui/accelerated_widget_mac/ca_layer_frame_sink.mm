// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/ca_layer_frame_sink.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#else
#import <UIKit/UIKit.h>
#include "ui/accelerated_widget_mac/ca_layer_frame_sink_provider.h"
#endif

namespace ui {

// static
CALayerFrameSink* CALayerFrameSink::FromAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
#if BUILDFLAG(IS_MAC)
  return AcceleratedWidgetMac::Get(widget);
#else
  id object = (__bridge id)(void*)widget;
  if ([object isKindOfClass:[CALayerFrameSinkProvider class]]) {
    return [(CALayerFrameSinkProvider*)object frameSink];
  }
  return nullptr;
#endif
}

}  // namespace ui

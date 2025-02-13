// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/accelerated_widget_mac/ca_layer_frame_sink_provider.h"

#include <map>

namespace {

std::map<gfx::AcceleratedWidget, CALayerFrameSinkProvider*>&
WidgetToSinkProviderMap() {
  static std::map<gfx::AcceleratedWidget, CALayerFrameSinkProvider*> map;
  return map;
}

}  // namespace

@implementation CALayerFrameSinkProvider {
  uint64_t _view_handle;
}

- (ui::CALayerFrameSink*)frameSink {
  return nil;
}

- (gfx::AcceleratedWidget)viewHandle {
  return _view_handle;
}

- (id)init {
  self = [super init];
  if (self) {
    static uint64_t last_sequence_number = 0;
    _view_handle = ++last_sequence_number;
    WidgetToSinkProviderMap().insert(std::make_pair(_view_handle, self));
  }
  return self;
}

- (void)dealloc {
  WidgetToSinkProviderMap().erase(_view_handle);
}

+ (CALayerFrameSinkProvider*)lookupByHandle:(uint64_t)viewHandle {
  auto found = WidgetToSinkProviderMap().find(viewHandle);
  if (found == WidgetToSinkProviderMap().end()) {
    return nullptr;
  }
  return found->second;
}
@end

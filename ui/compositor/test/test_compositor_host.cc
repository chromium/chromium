// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"
#include "ui/compositor/compositor.h"

namespace ui {

cc::LayerTreeHost* TestCompositorHost::GetLayerTreeHost() {
  return GetCompositor()->host_.get();
}

}  // namespace ui

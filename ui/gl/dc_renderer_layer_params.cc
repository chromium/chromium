// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/dc_renderer_layer_params.h"

#include "ui/gl/gl_image.h"

namespace ui {

DCRendererLayerParams::DCRendererLayerParams() = default;

DCRendererLayerParams::~DCRendererLayerParams() {
  for (auto& callback : release_image_cb) {
    if (callback)
      std::move(callback).Run();
  }
}

}  // namespace ui

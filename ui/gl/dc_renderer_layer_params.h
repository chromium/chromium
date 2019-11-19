// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DC_RENDERER_LAYER_PARAMS_H_
#define UI_GL_DC_RENDERER_LAYER_PARAMS_H_

#include <array>

#include "base/memory/ref_counted.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_export.h"

namespace gl {
class GLImage;
}

namespace ui {

struct GL_EXPORT DCRendererLayerParams {
  DCRendererLayerParams();
  DCRendererLayerParams(const DCRendererLayerParams& other);
  DCRendererLayerParams& operator=(const DCRendererLayerParams& other);
  ~DCRendererLayerParams();

  // Images to display in overlay.  There can either be two software video
  // buffers for Y and UV planes, an NV12 hardware video image, or a swap chain
  // image.  If a single image is specified, the second one must be nullptr.
  enum : size_t { kNumImages = 2 };
  using OverlayImages = std::array<scoped_refptr<gl::GLImage>, kNumImages>;
  OverlayImages images;

  // Stacking order relative to backbuffer which has z-order 0.
  int z_order = 1;

  // What part of the content to display in pixels.
  gfx::Rect content_rect;

  // Bounds of the overlay in pre-transform space.
  gfx::Rect quad_rect;

  // 2D flattened transform that maps |quad_rect| to root target space,
  // after applying the |quad_rect.origin()| as an offset.
  gfx::Transform transform;

  // If |is_clipped| is true, then clip to |clip_rect| in root target space.
  bool is_clipped = false;
  gfx::Rect clip_rect;

  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;
};

}  // namespace ui

#endif  // UI_GL_DC_RENDERER_LAYER_PARAMS_H_

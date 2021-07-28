// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_overlay.h"

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_image.h"

namespace gl {

GLSurfaceOverlay::GLSurfaceOverlay(int z_order,
                                   gfx::OverlayTransform transform,
                                   GLImage* image,
                                   const gfx::Rect& bounds_rect,
                                   const gfx::RectF& crop_rect,
                                   bool enable_blend,
                                   const gfx::Rect& damage_rect,
                                   std::unique_ptr<gfx::GpuFence> gpu_fence)
    : z_order_(z_order),
      transform_(transform),
      image_(image),
      bounds_rect_(bounds_rect),
      crop_rect_(crop_rect),
      enable_blend_(enable_blend),
      damage_rect_(damage_rect),
      gpu_fence_(std::move(gpu_fence)) {}

GLSurfaceOverlay::GLSurfaceOverlay(GLSurfaceOverlay&& other) = default;

GLSurfaceOverlay::~GLSurfaceOverlay() {}

bool GLSurfaceOverlay::ScheduleOverlayPlane(gfx::AcceleratedWidget widget) {
  std::vector<gfx::GpuFence> acquire_fences;
  if (gpu_fence_)
    acquire_fences.push_back(std::move(*gpu_fence_));

  auto pixmap = image_->GetNativePixmap();
  DCHECK(pixmap);
  return pixmap->ScheduleOverlayPlane(
      widget, z_order_, transform_, bounds_rect_, crop_rect_, enable_blend_,
      damage_rect_, std::move(acquire_fences), {});
}

void GLSurfaceOverlay::Flush() const {
  return image_->Flush();
}

}  // namespace gl

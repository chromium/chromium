// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_GL_SURFACE_CAST_H_
#define UI_OZONE_PLATFORM_CAST_GL_SURFACE_CAST_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"

namespace ui {

class GLOzoneEglCast;

class GLSurfaceCast : public gl::NativeViewGLSurfaceEGL {
 public:
  GLSurfaceCast(gfx::AcceleratedWidget widget, GLOzoneEglCast* parent);

  // gl::GLSurface:
  bool SupportsSwapBuffersWithBounds() override;
  gfx::SwapResult SwapBuffersWithBounds(const std::vector<gfx::Rect>& rects,
                                        PresentationCallback callback) override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            gl::GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  EGLConfig GetConfig() override;
  int GetBufferCount() const override;

 protected:
  ~GLSurfaceCast() override;

  gfx::AcceleratedWidget widget_;
  GLOzoneEglCast* parent_;
  bool supports_swap_buffer_with_bounds_;
  bool uses_triple_buffering_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceCast);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_GL_SURFACE_CAST_H_

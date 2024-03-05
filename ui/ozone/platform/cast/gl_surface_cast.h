// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_GL_SURFACE_CAST_H_
#define UI_OZONE_PLATFORM_CAST_GL_SURFACE_CAST_H_

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface_egl.h"

namespace ui {

class GLOzoneEglCast;

class GLSurfaceCast : public gl::NativeViewGLSurfaceEGL {
 public:
  GLSurfaceCast(gl::GLDisplayEGL* display,
                gfx::AcceleratedWidget widget,
                GLOzoneEglCast* parent);

  GLSurfaceCast(const GLSurfaceCast&) = delete;
  GLSurfaceCast& operator=(const GLSurfaceCast&) = delete;

  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  EGLConfig GetConfig() override;
  int GetBufferCount() const override;

 protected:
  ~GLSurfaceCast() override;

  gfx::AcceleratedWidget widget_;
  GLOzoneEglCast* parent_;
  bool uses_triple_buffering_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_GL_SURFACE_CAST_H_

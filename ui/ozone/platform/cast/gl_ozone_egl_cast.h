// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_GL_OZONE_EGL_CAST_H_
#define UI_OZONE_PLATFORM_CAST_GL_OZONE_EGL_CAST_H_

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/common/gl_ozone_egl.h"

namespace chromecast {
class CastEglPlatform;
}

namespace ui {

// GL implementation using EGL for Ozone cast platform.
class GLOzoneEglCast : public GLOzoneEGL {
 public:
  explicit GLOzoneEglCast(
      std::unique_ptr<chromecast::CastEglPlatform> egl_platform);
  ~GLOzoneEglCast() override;

  // GLOzoneEGL implementation:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override;
  intptr_t GetNativeDisplay() override;
  bool LoadGLES2Bindings(gl::GLImplementation implementation) override;

  intptr_t GetNativeWindow();
  bool ResizeDisplay(gfx::Size viewport_size);
  void TerminateDisplay();

 private:
  void CreateDisplayTypeAndWindowIfNeeded();
  void InitializeHardwareIfNeeded();

  bool hardware_initialized_ = false;
  void* display_type_ = 0;
  bool have_display_type_ = false;
  void* window_ = 0;
  gfx::Size display_size_;
  std::unique_ptr<chromecast::CastEglPlatform> egl_platform_;

  DISALLOW_COPY_AND_ASSIGN(GLOzoneEglCast);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_GL_OZONE_EGL_CAST_H_

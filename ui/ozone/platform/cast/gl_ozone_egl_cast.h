// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_GL_OZONE_EGL_CAST_H_
#define UI_OZONE_PLATFORM_CAST_GL_OZONE_EGL_CAST_H_

#include <stdint.h>
#include <memory>

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

  GLOzoneEglCast(const GLOzoneEglCast&) = delete;
  GLOzoneEglCast& operator=(const GLOzoneEglCast&) = delete;

  ~GLOzoneEglCast() override;

  // GLOzoneEGL implementation:
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLDisplay* display,
      const gfx::Size& size) override;
  gl::EGLDisplayPlatform GetNativeDisplay() override;
  bool LoadGLES2Bindings(
      const gl::GLImplementationParts& implementation) override;

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
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_GL_OZONE_EGL_CAST_H_

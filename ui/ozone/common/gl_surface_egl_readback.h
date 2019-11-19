// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_GL_SURFACE_EGL_READBACK_H_
#define UI_OZONE_COMMON_GL_SURFACE_EGL_READBACK_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface_egl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace ui {

// GLSurface implementation that renders into a pbuffer and then does a readback
// of pixels into memory. This is intended to be used with SwiftShader where
// there is no FBO implementation for Ozone.
class GLSurfaceEglReadback : public gl::PbufferGLSurfaceEGL {
 public:
  GLSurfaceEglReadback();

  // GLSurface implementation.
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  bool FlipsVertically() const override;

  // TODO(kylechar): Implement SupportsPostSubBuffer() and PostSubBuffer().

 protected:
  ~GLSurfaceEglReadback() override;

  // Implementations should override this, use the pixels data and then return
  // true if succesful. Should return true on succesful swap or false on swap
  // failure.
  virtual bool HandlePixels(uint8_t* pixels);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<uint8_t[]> pixels_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceEglReadback);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_GL_SURFACE_EGL_READBACK_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_GL_SURFACE_EGL_READBACK_H_
#define UI_OZONE_COMMON_GL_SURFACE_EGL_READBACK_H_

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
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
  explicit GLSurfaceEglReadback(gl::GLDisplayEGL* display);

  GLSurfaceEglReadback(const GLSurfaceEglReadback&) = delete;
  GLSurfaceEglReadback& operator=(const GLSurfaceEglReadback&) = delete;

  // GLSurface implementation.
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback,
                              gfx::FrameData data) override;
  gfx::SurfaceOrigin GetOrigin() const override;

  // TODO(kylechar): Implement SupportsPostSubBuffer() and PostSubBuffer().

 protected:
  ~GLSurfaceEglReadback() override;

  // Implementations should override this, use the pixels data and then return
  // true if succesful. Should return true on succesful swap or false on swap
  // failure.
  //
  // TODO(danakj): This method should take a span, like ReadPixels.
  UNSAFE_BUFFER_USAGE virtual bool HandlePixels(uint8_t* pixels);

  // Reads pixels with glReadPixels from fbo to |buffer|.
  void ReadPixels(base::span<uint8_t> buffer);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::HeapArray<uint8_t> pixels_;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_GL_SURFACE_EGL_READBACK_H_

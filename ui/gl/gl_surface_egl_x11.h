// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_X11_H_
#define UI_GL_GL_SURFACE_EGL_X11_H_

#include <stdint.h>

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

// Encapsulates an EGL surface bound to a view using the X Window System.
class GL_EXPORT NativeViewGLSurfaceEGLX11 : public NativeViewGLSurfaceEGL,
                                            public x11::EventObserver {
 public:
  NativeViewGLSurfaceEGLX11(GLDisplayEGL* display, x11::Window window);
  NativeViewGLSurfaceEGLX11(const NativeViewGLSurfaceEGLX11& other) = delete;
  NativeViewGLSurfaceEGLX11& operator=(const NativeViewGLSurfaceEGLX11& rhs) =
      delete;

  // NativeViewGLSurfaceEGL overrides.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback,
                              gfx::FrameData data) override;
  EGLint GetNativeVisualID() const override;

 protected:
  ~NativeViewGLSurfaceEGLX11() override;

  x11::Connection* GetXNativeConnection() const;

 private:
  // NativeViewGLSurfaceEGL overrides:
  std::unique_ptr<gfx::VSyncProvider> CreateVsyncProviderInternal() override;

  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override;

  std::vector<x11::Window> children_;

  // Indicates if the dispatcher has been set.
  bool dispatcher_set_ = false;

  bool has_swapped_buffers_ = false;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_X11_H_

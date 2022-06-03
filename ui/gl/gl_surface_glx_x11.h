// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_GLX_X11_H_
#define UI_GL_GL_SURFACE_GLX_X11_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_glx.h"

namespace gl {

// X11 specific implementation of GLX surface. Registers as a
// PlatformEventDispatcher to handle XEvents.
class GL_EXPORT GLSurfaceGLXX11 : public NativeViewGLSurfaceGLX,
                                  public x11::EventObserver {
 public:
  explicit GLSurfaceGLXX11(gfx::AcceleratedWidget window);

  GLSurfaceGLXX11(const GLSurfaceGLXX11&) = delete;
  GLSurfaceGLXX11& operator=(const GLSurfaceGLXX11&) = delete;

 protected:
  ~GLSurfaceGLXX11() override;

  // NativeViewGLSurfaceGLX:
  void RegisterEvents() override;
  void UnregisterEvents() override;

  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_GLX_X11_H_

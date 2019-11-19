// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_GL_SURFACE_GLX_OZONE_H_
#define UI_OZONE_PLATFORM_X11_GL_SURFACE_GLX_OZONE_H_

#include "base/macros.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gl/gl_surface_glx.h"

namespace ui {

// Ozone specific implementation of GLX surface. Registers as a XEventDispatcher
// to handle XEvents.
class GLSurfaceGLXOzone : public gl::NativeViewGLSurfaceGLX,
                          public XEventDispatcher {
 public:
  explicit GLSurfaceGLXOzone(gfx::AcceleratedWidget window);

 protected:
  ~GLSurfaceGLXOzone() override;

  // NativeViewGLSurfaceGLX:
  void RegisterEvents() override;
  void UnregisterEvents() override;

  // XEventDispatcher:
  bool DispatchXEvent(XEvent* xevent) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceGLXOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_GL_SURFACE_GLX_OZONE_H_

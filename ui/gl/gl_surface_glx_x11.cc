// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_glx_x11.h"

#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

using ui::X11EventSource;

namespace gl {

GLSurfaceGLXX11::GLSurfaceGLXX11(gfx::AcceleratedWidget window)
    : NativeViewGLSurfaceGLX(window) {}

GLSurfaceGLXX11::~GLSurfaceGLXX11() {
  Destroy();
}

void GLSurfaceGLXX11::RegisterEvents() {
  // Can be null in tests, when we don't care about Exposes.
  if (X11EventSource::HasInstance()) {
    x11::Connection::Get()->ChangeWindowAttributes(
        {.window = static_cast<x11::Window>(window()),
         .event_mask = x11::EventMask::Exposure});
    X11EventSource::GetInstance()->AddXEventDispatcher(this);
  }
}

void GLSurfaceGLXX11::UnregisterEvents() {
  if (X11EventSource::HasInstance())
    X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

bool GLSurfaceGLXX11::DispatchXEvent(x11::Event* event) {
  if (!CanHandleEvent(event))
    return false;
  ForwardExposeEvent(event);
  return true;
}

}  // namespace gl

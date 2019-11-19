// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/gl_surface_glx_ozone.h"

#include "ui/gfx/x/x11.h"

namespace ui {

GLSurfaceGLXOzone::GLSurfaceGLXOzone(gfx::AcceleratedWidget window)
    : NativeViewGLSurfaceGLX(window) {}

GLSurfaceGLXOzone::~GLSurfaceGLXOzone() {
  Destroy();
}

void GLSurfaceGLXOzone::RegisterEvents() {
  auto* event_source = X11EventSource::GetInstance();
  // Can be null in tests, when we don't care about Exposes.
  if (event_source) {
    XSelectInput(gfx::GetXDisplay(), window(), ExposureMask);
    event_source->AddXEventDispatcher(this);
  }
}

void GLSurfaceGLXOzone::UnregisterEvents() {
  auto* event_source = X11EventSource::GetInstance();
  if (event_source)
    event_source->RemoveXEventDispatcher(this);
}

bool GLSurfaceGLXOzone::DispatchXEvent(XEvent* event) {
  if (!CanHandleEvent(event))
    return false;

  ForwardExposeEvent(event);
  return true;
}

}  // namespace ui

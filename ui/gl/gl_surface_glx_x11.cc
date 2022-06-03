// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_glx_x11.h"

#include "ui/gfx/x/future.h"

namespace gl {

GLSurfaceGLXX11::GLSurfaceGLXX11(gfx::AcceleratedWidget window)
    : NativeViewGLSurfaceGLX(window) {}

GLSurfaceGLXX11::~GLSurfaceGLXX11() {
  Destroy();
}

void GLSurfaceGLXX11::RegisterEvents() {
  // Can be null in tests, when we don't care about Exposes.
  auto* connection = x11::Connection::Get();

  connection->ChangeWindowAttributes(x11::ChangeWindowAttributesRequest{
      .window = static_cast<x11::Window>(window()),
      .event_mask = x11::EventMask::Exposure});

  connection->AddEventObserver(this);
}

void GLSurfaceGLXX11::UnregisterEvents() {
  x11::Connection::Get()->RemoveEventObserver(this);
}

void GLSurfaceGLXX11::OnEvent(const x11::Event& event) {
  if (CanHandleEvent(event))
    ForwardExposeEvent(event);
}

}  // namespace gl

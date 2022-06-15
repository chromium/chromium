// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_display.h"

#include "base/notreached.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_GLX)
#include "ui/gl/glx_util.h"
#endif  // defined(USE_GLX)

namespace gl {

GLDisplay::GLDisplay(uint64_t system_device_id)
    : system_device_id_(system_device_id) {}

GLDisplay::~GLDisplay() = default;

#if defined(USE_EGL)
GLDisplayEGL::GLDisplayEGL(uint64_t system_device_id)
    : GLDisplay(system_device_id) {
  ext = std::make_unique<DisplayExtensionsEGL>();
  display_ = EGL_NO_DISPLAY;
}

GLDisplayEGL::~GLDisplayEGL() = default;

EGLDisplay GLDisplayEGL::GetDisplay() {
  return display_;
}

void GLDisplayEGL::SetDisplay(EGLDisplay display) {
  display_ = display;
}

EGLDisplay GLDisplayEGL::GetHardwareDisplay() {
  return GetDisplay();
}

EGLNativeDisplayType GLDisplayEGL::GetNativeDisplay() {
  return native_display.GetDisplay();
}

DisplayType GLDisplayEGL::GetDisplayType() {
  return display_type;
}

// static
GLDisplayEGL* GLDisplayEGL::GetDisplayForCurrentContext() {
  GLContext* context = GLContext::GetCurrent();
  return context ? context->GetGLDisplayEGL() : nullptr;
}

bool GLDisplayEGL::IsEGLSurfacelessContextSupported() {
  return egl_surfaceless_context_supported;
}

bool GLDisplayEGL::IsEGLContextPrioritySupported() {
  return egl_context_priority_supported;
}

bool GLDisplayEGL::IsAndroidNativeFenceSyncSupported() {
  return egl_android_native_fence_sync_supported;
}

bool GLDisplayEGL::IsANGLEExternalContextAndSurfaceSupported() {
  return this->ext->b_EGL_ANGLE_external_context_and_surface;
}
#endif  // defined(USE_EGL)

#if defined(USE_GLX)
GLDisplayX11::GLDisplayX11(uint64_t system_device_id)
    : GLDisplay(system_device_id) {}

GLDisplayX11::~GLDisplayX11() = default;

void* GLDisplayX11::GetDisplay() {
  return x11::Connection::Get()->GetXlibDisplay();
}
#endif  // defined(USE_GLX)

}  // namespace gl

// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_display.h"
#include "base/notreached.h"

#if defined(USE_GLX)
#include "ui/gl/glx_util.h"
#endif  // defined(USE_GLX)

#if defined(USE_EGL)
#define EGL_NO_DISPLAY 0
#endif

namespace gl {

GLDisplay::GLDisplay() = default;

GLDisplay::~GLDisplay() = default;

#if defined(USE_EGL)
GLDisplayEGL::GLDisplayEGL() {
  display_ = EGL_NO_DISPLAY;
}

GLDisplayEGL::GLDisplayEGL(EGLDisplay display) {
  display_ = display;
}

GLDisplayEGL::~GLDisplayEGL() = default;

EGLDisplay GLDisplayEGL::GetDisplay() {
  return display_;
}

void GLDisplayEGL::SetDisplay(EGLDisplay display) {
  display_ = display;
}

#endif  // defined(USE_EGL)

#if defined(USE_GLX)
GLDisplayX11::GLDisplayX11() = default;

GLDisplayX11::~GLDisplayX11() = default;

void* GLDisplayX11::GetDisplay() {
  return x11::Connection::Get()->GetXlibDisplay();
}
#endif  // defined(USE_GLX)

}  // namespace gl
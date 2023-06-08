// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/native_pixmap_egl_x11_binding_helper.h"

#include <memory>

#include "ui/gfx/x/connection.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

inline EGLDisplay FromXDisplay() {
  auto* x_display = x11::Connection::Get()->GetXlibDisplay().display();
  return eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(x_display));
}

NativePixmapEGLX11BindingHelper::NativePixmapEGLX11BindingHelper()
    : display_(FromXDisplay()) {}

NativePixmapEGLX11BindingHelper::~NativePixmapEGLX11BindingHelper() {
  if (surface_) {
    eglDestroySurface(display_, surface_);
  }
}

bool NativePixmapEGLX11BindingHelper::Initialize(x11::Pixmap pixmap) {
  if (eglInitialize(display_, nullptr, nullptr) != EGL_TRUE) {
    return false;
  }

  EGLint attribs[] = {EGL_BUFFER_SIZE,
                      32,
                      EGL_ALPHA_SIZE,
                      8,
                      EGL_BLUE_SIZE,
                      8,
                      EGL_GREEN_SIZE,
                      8,
                      EGL_RED_SIZE,
                      8,
                      EGL_SURFACE_TYPE,
                      EGL_PIXMAP_BIT,
                      EGL_BIND_TO_TEXTURE_RGBA,
                      EGL_TRUE,
                      EGL_NONE};

  EGLint num_configs;
  EGLConfig config = nullptr;

  if ((eglChooseConfig(display_, attribs, &config, 1, &num_configs) !=
       EGL_TRUE) ||
      !num_configs) {
    return false;
  }

  std::vector<EGLint> attrs = {EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
                               EGL_TEXTURE_TARGET, EGL_TEXTURE_2D, EGL_NONE};

  surface_ = eglCreatePixmapSurface(
      display_, config, static_cast<EGLNativePixmapType>(pixmap), attrs.data());
  return surface_ != EGL_NO_SURFACE;
}

bool NativePixmapEGLX11BindingHelper::BindTexImage(unsigned target) {
  if (!surface_) {
    return false;
  }

  // Requires TEXTURE_2D target.
  if (target != GL_TEXTURE_2D) {
    return false;
  }

  if (eglBindTexImage(display_, surface_, EGL_BACK_BUFFER) != EGL_TRUE) {
    return false;
  }

  return true;
}

void NativePixmapEGLX11BindingHelper::ReleaseEGLImage() {
  DCHECK_NE(nullptr, surface_);

  eglReleaseTexImage(display_, surface_, EGL_BACK_BUFFER);
}

}  // namespace gl

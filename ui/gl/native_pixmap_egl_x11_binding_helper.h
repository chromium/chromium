// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_NATIVE_PIXMAP_EGL_X11_BINDING_HELPER_H_
#define UI_GL_NATIVE_PIXMAP_EGL_X11_BINDING_HELPER_H_

#include <stdint.h>

#include "ui/gfx/x/glx.h"
#include "ui/gl/gl_export.h"

typedef void* EGLSurface;
typedef void* EGLDisplay;

namespace gl {

class GL_EXPORT NativePixmapEGLX11BindingHelper {
 public:
  NativePixmapEGLX11BindingHelper();

  NativePixmapEGLX11BindingHelper(const NativePixmapEGLX11BindingHelper&) =
      delete;
  NativePixmapEGLX11BindingHelper& operator=(
      const NativePixmapEGLX11BindingHelper&) = delete;

  bool Initialize(x11::Pixmap pixmap);

  // Binds image to texture currently bound to |target|. Returns true on
  // success.
  bool BindTexImage(unsigned target);

  // Releases the image that was bound via BindTexImage().
  void ReleaseEGLImage();

  ~NativePixmapEGLX11BindingHelper();

 private:
  EGLSurface surface_ = nullptr;
  EGLDisplay display_;
};

}  // namespace gl

#endif  // UI_GL_NATIVE_PIXMAP_EGL_X11_BINDING_HELPER_H_

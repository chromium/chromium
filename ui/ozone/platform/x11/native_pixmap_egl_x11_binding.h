// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_
#define UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_

#include <memory>

#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/glx.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"

typedef void* EGLSurface;
typedef void* EGLDisplay;

namespace ui {

// A binding maintained between NativePixmap and GL Textures in Ozone that works
// within the context of X11.
class NativePixmapEGLX11Binding : public NativePixmapGLBinding {
 public:
  explicit NativePixmapEGLX11Binding(gfx::BufferFormat format);
  ~NativePixmapEGLX11Binding() override;

  static bool IsBufferFormatSupported(gfx::BufferFormat format);

  static std::unique_ptr<NativePixmapGLBinding> Create(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::Size plane_size,
      GLenum target,
      GLuint texture_id);

  static bool CanImportNativeGLXPixmap();

 private:
  bool Initialize(x11::Pixmap pixmap);

  // Binds image to texture currently bound to |target|. Returns true on
  // success.
  bool BindTexture(GLenum target, GLuint texture_id);

  EGLSurface surface_ = nullptr;
  EGLDisplay display_;

  x11::Pixmap pixmap_ = x11::Pixmap::None;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_

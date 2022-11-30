// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_GLX_BINDING_H_
#define UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_GLX_BINDING_H_

#include "ui/ozone/public/native_pixmap_gl_binding.h"

namespace ui {

// A binding maintained between GLImageGLXNativePixmap and GL Textures in Ozone.
// This binding is used for ChromeOS-on-Linux and for Linux/Ozone/X11 with
// Drm/Kms.
class NativePixmapGLXBinding : public NativePixmapGLBinding {
 public:
  NativePixmapGLXBinding();
  ~NativePixmapGLXBinding() override;

  static std::unique_ptr<NativePixmapGLBinding> Create(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::BufferPlane plane,
      gfx::Size plane_size,
      GLenum target,
      GLuint texture_id);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11C_NATIVE_PIXMAP_GLX_BINDING_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/native_pixmap_egl_x11_binding.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gl/gl_image_egl_pixmap.h"
#include "ui/gl/glx_util.h"

namespace ui {

NativePixmapEGLX11Binding::NativePixmapEGLX11Binding() = default;
NativePixmapEGLX11Binding::~NativePixmapEGLX11Binding() = default;

// static
std::unique_ptr<NativePixmapGLBinding> NativePixmapEGLX11Binding::Create(
    scoped_refptr<gfx::NativePixmap> native_pixmap,
    gfx::BufferFormat plane_format,
    gfx::Size plane_size,
    GLenum target,
    GLuint texture_id) {
  auto gl_image =
      base::MakeRefCounted<gl::GLImageEGLPixmap>(plane_size, plane_format);
  x11::Pixmap pixmap = gl::XPixmapFromNativePixmap(
      *static_cast<gfx::NativePixmapDmaBuf*>(native_pixmap.get()),
      plane_format);

  // Initialize the image calling eglCreatePixmapSurface.
  if (!gl_image->Initialize(std::move(pixmap))) {
    LOG(ERROR) << "Unable to initialize GL image from pixmap";
    return nullptr;
  }

  auto binding = std::make_unique<NativePixmapEGLX11Binding>();
  if (!binding->BindTexture(std::move(gl_image), target, texture_id)) {
    return nullptr;
  }

  return binding;
}

}  // namespace ui

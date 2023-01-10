// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/native_pixmap_egl_binding.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gl/gl_image_native_pixmap.h"

namespace ui {

NativePixmapEGLBinding::NativePixmapEGLBinding() = default;
NativePixmapEGLBinding::~NativePixmapEGLBinding() = default;

// static
std::unique_ptr<NativePixmapGLBinding> NativePixmapEGLBinding::Create(
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat plane_format,
    gfx::BufferPlane plane,
    gfx::Size plane_size,
    const gfx::ColorSpace& color_space,
    GLenum target,
    GLuint texture_id) {
  auto gl_image = gl::GLImageNativePixmap::CreateForPlane(
      plane_size, plane_format, plane, std::move(pixmap), color_space);
  if (!gl_image) {
    LOG(ERROR) << "Unable to initialize GL image from pixmap";
    return nullptr;
  }

  auto binding = std::make_unique<NativePixmapEGLBinding>();
  if (!binding->BindTexture(std::move(gl_image), target, texture_id)) {
    return nullptr;
  }

  return binding;
}

}  // namespace ui

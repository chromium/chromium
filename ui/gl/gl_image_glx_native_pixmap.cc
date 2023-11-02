// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_glx_native_pixmap.h"

#include "ui/gfx/buffer_types.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/dri3.h"
#include "ui/gfx/x/glx.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/glx_util.h"

namespace gl {

GLImageGLXNativePixmap::GLImageGLXNativePixmap(const gfx::Size& size,
                                               gfx::BufferFormat format,
                                               gfx::BufferPlane plane)
    : GLImageGLX(size, format) {
  DCHECK_EQ(plane, gfx::BufferPlane::DEFAULT);
}

GLImageGLXNativePixmap::~GLImageGLXNativePixmap() = default;

bool GLImageGLXNativePixmap::Initialize(
    scoped_refptr<gfx::NativePixmap> pixmap) {
  native_pixmap_ = pixmap;

  return GLImageGLX::Initialize(XPixmapFromNativePixmap(
      *static_cast<gfx::NativePixmapDmaBuf*>(native_pixmap_.get()), format()));
}

bool GLImageGLXNativePixmap::CanImportNativePixmap() {
  auto* conn = x11::Connection::Get();
  return conn->dri3().present() && conn->glx().present();
}

}  // namespace gl

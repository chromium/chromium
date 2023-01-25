// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/native_pixmap_egl_x11_binding.h"

#include <GL/gl.h>

#include <unistd.h>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/dri3.h"
#include "ui/gfx/x/future.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_image_egl_pixmap.h"

namespace gl {

namespace {
int Depth(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGR_565:
      return 16;
    case gfx::BufferFormat::BGRX_8888:
      return 24;
    case gfx::BufferFormat::BGRA_1010102:
    case gfx::BufferFormat::BGRA_8888:
      return 32;
    default:
      NOTREACHED();
      return 0;
  }
}
int Bpp(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGR_565:
      return 16;
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_1010102:
    case gfx::BufferFormat::BGRA_8888:
      return 32;
    default:
      NOTREACHED();
      return 0;
  }
}
x11::Pixmap XPixmapFromNativePixmap(
    const gfx::NativePixmapDmaBuf& native_pixmap,
    gfx::BufferFormat buffer_format) {
  int depth = Depth(buffer_format);
  int bpp = Bpp(buffer_format);
  auto fd = HANDLE_EINTR(dup(native_pixmap.GetDmaBufFd(0)));
  if (fd < 0)
    return x11::Pixmap::None;
  x11::RefCountedFD ref_counted_fd(fd);

  auto* connection = x11::Connection::Get();
  x11::Pixmap pixmap_id = connection->GenerateId<x11::Pixmap>();
  // This should be synced. Otherwise, glXCreatePixmap may fail on ChromeOS
  // with "failed to create a drawable" error.
  connection->dri3()
      .PixmapFromBuffer(pixmap_id, connection->default_root(),
                        native_pixmap.GetDmaBufPlaneSize(0),
                        native_pixmap.GetBufferSize().width(),
                        native_pixmap.GetBufferSize().height(),
                        native_pixmap.GetDmaBufPitch(0), depth, bpp,
                        ref_counted_fd)
      .Sync();
  return pixmap_id;
}
}  // namespace

}  // namespace gl

namespace ui {

NativePixmapEGLX11Binding::NativePixmapEGLX11Binding(
    scoped_refptr<gl::GLImageEGLPixmap> gl_image,
    gfx::BufferFormat format)
    : gl_image_(std::move(gl_image)), format_(format) {}
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

  auto binding = std::make_unique<NativePixmapEGLX11Binding>(
      std::move(gl_image), plane_format);
  if (!binding->BindTexture(target, texture_id)) {
    return nullptr;
  }

  return binding;
}

bool NativePixmapEGLX11Binding::BindTexture(GLenum target, GLuint texture_id) {
  return NativePixmapGLBinding::BindTexture(gl_image_.get(), target,
                                            texture_id);
}

GLuint NativePixmapEGLX11Binding::GetInternalFormat() {
  return gl::BufferFormatToGLInternalFormat(format_);
}

GLenum NativePixmapEGLX11Binding::GetDataType() {
  return GL_UNSIGNED_BYTE;
}

}  // namespace ui

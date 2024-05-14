// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/native_pixmap_egl_x11_binding.h"

#include <GL/gl.h>

#include <unistd.h>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/dri3.h"
#include "ui/gfx/x/future.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"

namespace gl {

namespace {
bool IsFormatSupported(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGRA_8888:
      return true;
    default:
      return false;
  }
}

uint8_t Depth(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGRA_8888:
      return 32;
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

uint8_t Bpp(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGRA_8888:
      return 32;
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

x11::Pixmap XPixmapFromNativePixmap(const gfx::NativePixmap& native_pixmap,
                                    gfx::BufferFormat buffer_format) {
  const uint8_t depth = Depth(buffer_format);
  const uint8_t bpp = Bpp(buffer_format);
  const auto fd = HANDLE_EINTR(dup(native_pixmap.GetDmaBufFd(0)));
  if (fd < 0) {
    VPLOG(1) << "Could not import the dma-buf as an XPixmap because the FD "
                "couldn't be dup()ed";
    return x11::Pixmap::None;
  }
  x11::RefCountedFD ref_counted_fd(fd);

  uint32_t buffer_byte_size;
  if (!base::IsValueInRangeForNumericType<uint32_t>(
          native_pixmap.GetDmaBufPlaneSize(0))) {
    VLOG(1) << "Could not import the dma-buf as an XPixmap because the "
               "dma-buf's byte size is out-of-range";
    return x11::Pixmap::None;
  }
  buffer_byte_size =
      base::checked_cast<uint32_t>(native_pixmap.GetDmaBufPlaneSize(0));

  uint16_t width;
  if (!base::IsValueInRangeForNumericType<uint16_t>(
          native_pixmap.GetBufferSize().width())) {
    VLOG(1) << "Could not import the dma-buf as an XPixmap because the "
               "dma-buf's width is out-of-range";
    return x11::Pixmap::None;
  }
  width = base::checked_cast<uint16_t>(native_pixmap.GetBufferSize().width());

  uint16_t height;
  if (!base::IsValueInRangeForNumericType<uint16_t>(
          native_pixmap.GetBufferSize().height())) {
    VLOG(1) << "Could not import the dma-buf as an XPixmap because the "
               "dma-buf's width is out-of-range";
    return x11::Pixmap::None;
  }
  height = base::checked_cast<uint16_t>(native_pixmap.GetBufferSize().height());

  uint16_t stride;
  if (!base::IsValueInRangeForNumericType<uint16_t>(
          native_pixmap.GetDmaBufPitch(0))) {
    VLOG(1) << "Could not import the dma-buf as an XPixmap because the "
               "dma-buf's width is out-of-range";
    return x11::Pixmap::None;
  }
  stride = base::checked_cast<uint16_t>(native_pixmap.GetDmaBufPitch(0));

  auto* connection = x11::Connection::Get();
  const x11::Pixmap pixmap_id = connection->GenerateId<x11::Pixmap>();
  if (pixmap_id == x11::Pixmap::None) {
    VLOG(1) << "Could not import the dma-buf as an XPixmap because an ID "
               "couldn't be generated";
    return x11::Pixmap::None;
  }

  // TODO(crbug.com/40254955): this was made Sync() reportedly because
  // glXCreatePixmap() would fail on ChromeOS with "failed to create a drawable"
  // otherwise. Today, ChromeOS doesn't use X11, so that reason is obsolete. I
  // tried removing the Sync() for Linux, tested with hardware decoding a 4k
  // video, and saw a ~6% improvement in total power consumption without any
  // visible issues. We should evaluate removing this Sync() safely.
  auto response = connection->dri3()
                      .PixmapFromBuffer(pixmap_id, connection->default_root(),
                                        buffer_byte_size, width, height, stride,
                                        depth, bpp, ref_counted_fd)
                      .Sync();
  if (response.error) {
    VLOG(1) << "Could not import the dma-buf as an XPixmap because "
               "PixmapFromBuffer() failed; error: "
            << response.error->ToString();
    return x11::Pixmap::None;
  }

  return pixmap_id;
}

inline EGLDisplay FromXDisplay() {
  auto* x_display = x11::Connection::Get()->GetXlibDisplay().display();
  return eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(x_display));
}

}  // namespace

}  // namespace gl

namespace ui {

NativePixmapEGLX11Binding::NativePixmapEGLX11Binding(gfx::BufferFormat format)
    : display_(gl::FromXDisplay()) {}

NativePixmapEGLX11Binding::~NativePixmapEGLX11Binding() {
  if (surface_) {
    eglDestroySurface(display_, surface_);
  }

  if (pixmap_ != x11::Pixmap::None) {
    auto* connection = x11::Connection::Get();
    connection->FreePixmap({pixmap_});
  }
}

bool NativePixmapEGLX11Binding::IsBufferFormatSupported(
    gfx::BufferFormat format) {
  return gl::IsFormatSupported(format);
}

bool NativePixmapEGLX11Binding::Initialize(x11::Pixmap pixmap) {
  CHECK_NE(pixmap, x11::Pixmap::None);
  pixmap_ = pixmap;

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

// static
std::unique_ptr<NativePixmapGLBinding> NativePixmapEGLX11Binding::Create(
    scoped_refptr<gfx::NativePixmap> native_pixmap,
    gfx::BufferFormat plane_format,
    gfx::Size plane_size,
    GLenum target,
    GLuint texture_id) {
  if (native_pixmap->GetBufferFormat() != plane_format ||
      !gl::IsFormatSupported(plane_format)) {
    VLOG(1) << "Format " << gfx::BufferFormatToString(plane_format)
            << " is unsupported or does not match the NativePixmap's format ("
            << gfx::BufferFormatToString(native_pixmap->GetBufferFormat())
            << ")";
    return nullptr;
  }

  if (native_pixmap->GetBufferSize() != plane_size) {
    VLOG(1) << "The native pixmap size ("
            << native_pixmap->GetBufferSize().ToString()
            << ") does not match |plane_size| (" << plane_size.ToString()
            << ")";
    return nullptr;
  }

  if (target != GL_TEXTURE_2D) {
    VLOG(1) << "GL target " << target << " is unsupported";
    return nullptr;
  }

  auto binding = std::make_unique<NativePixmapEGLX11Binding>(plane_format);
  x11::Pixmap pixmap =
      gl::XPixmapFromNativePixmap(*native_pixmap, plane_format);
  if (pixmap == x11::Pixmap::None) {
    return nullptr;
  }

  // Transfer the ownership of `pixmap` to `NativePixmapEGLX11Binding`.
  if (!binding->Initialize(std::move(pixmap))) {
    VLOG(1) << "Unable to initialize binding from pixmap";
    return nullptr;
  }

  if (!binding->BindTexture(target, texture_id)) {
    VLOG(1) << "Unable to bind the GL texture";
    return nullptr;
  }

  return binding;
}

// static
bool NativePixmapEGLX11Binding::CanImportNativeGLXPixmap() {
  auto* conn = x11::Connection::Get();
  return conn->dri3().present() && conn->glx().present();
}

bool NativePixmapEGLX11Binding::BindTexture(GLenum target, GLuint texture_id) {
  gl::ScopedTextureBinder binder(base::strict_cast<unsigned int>(target),
                                 base::strict_cast<unsigned int>(texture_id));

  if (!surface_) {
    LOG(ERROR) << "No surface, cannot bind to target = " << target;
    return false;
  }

  // Requires TEXTURE_2D target.
  if (target != GL_TEXTURE_2D) {
    LOG(ERROR) << "Cannot bind to unsupported target = " << target;
    return false;
  }

  if (eglBindTexImage(display_, surface_, EGL_BACK_BUFFER) != EGL_TRUE) {
    LOG(ERROR) << "Unable to bind image to target = " << target;
    return false;
  }

  return true;
}

}  // namespace ui

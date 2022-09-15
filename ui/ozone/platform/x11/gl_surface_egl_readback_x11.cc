// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/gl_surface_egl_readback_x11.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

namespace {

constexpr x11::GraphicsContext kNoGC = x11::GraphicsContext{};

}

GLSurfaceEglReadbackX11::GLSurfaceEglReadbackX11(gl::GLDisplayEGL* display,
                                                 gfx::AcceleratedWidget window)
    : GLSurfaceEglReadback(display),
      window_(static_cast<x11::Window>(window)),
      connection_(x11::Connection::Get()) {}

bool GLSurfaceEglReadbackX11::Initialize(gl::GLSurfaceFormat format) {
  if (!GLSurfaceEglReadback::Initialize(format))
    return false;

  // We don't need to reinitialize |window_graphics_context_|.
  if (window_graphics_context_ != kNoGC)
    return true;

  window_graphics_context_ = connection_->GenerateId<x11::GraphicsContext>();
  auto gc_future = connection_->CreateGC({window_graphics_context_, window_});

  if (auto attributes = connection_->GetWindowAttributes({window_}).Sync()) {
    visual_ = attributes->visual;
  } else {
    DLOG(ERROR) << "Failed to get attributes for window "
                << static_cast<uint32_t>(window_);
    Destroy();
    return false;
  }

  if (gc_future.Sync().error) {
    DLOG(ERROR) << "XCreateGC failed";
    Destroy();
    return false;
  }

  return true;
}

void GLSurfaceEglReadbackX11::Destroy() {
  if (window_graphics_context_ != kNoGC) {
    connection_->FreeGC({window_graphics_context_});
    window_graphics_context_ = kNoGC;
  }

  connection_->Sync();

  GLSurfaceEglReadback::Destroy();
}

GLSurfaceEglReadbackX11::~GLSurfaceEglReadbackX11() {
  Destroy();
}

bool GLSurfaceEglReadbackX11::HandlePixels(uint8_t* pixels) {
  SkImageInfo image_info =
      SkImageInfo::Make(GetSize().width(), GetSize().height(),
                        kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  SkPixmap pixmap(image_info, pixels, image_info.minRowBytes());

  // Copy pixels into pixmap and then update the XWindow.
  const gfx::Size size = GetSize();
  DrawPixmap(connection_, visual_, window_, window_graphics_context_, pixmap, 0,
             0, 0, 0, size.width(), size.height());

  return true;
}

}  // namespace ui

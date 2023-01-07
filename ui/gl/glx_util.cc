// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/glx_util.h"

#include <unistd.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/x/dri3.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/glx.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

namespace {

x11::Glx::FbConfig GetConfigForWindow(x11::Connection* conn,
                                      x11::Window window) {
  x11::VisualId visual;
  if (auto attrs = conn->GetWindowAttributes({window}).Sync()) {
    visual = attrs->visual;
  } else {
    LOG(ERROR) << "GetWindowAttributes failed for window "
               << static_cast<uint32_t>(window) << ".";
    return {};
  }

  if (auto configs =
          conn->glx()
              .GetFBConfigs({static_cast<uint32_t>(conn->DefaultScreenId())})
              .Sync()) {
    // The returned property_list is a table consisting of
    // 2 * num_FB_configs * num_properties uint32_t's.  Each entry in the table
    // is a key-value pair.  For example, if we have 2 FB configs and 3
    // properties, then the table would be laid out as such:
    //
    // k(c, p) = key for c'th config of p'th property
    // v(c, p) = value for c'th config of p'th property
    //
    // | k(1, 1) | v(1, 1) | k(1, 2) | v(1, 2) | k(1, 3) | v(1, 3) |
    // | k(2, 1) | v(2, 1) | k(2, 2) | v(2, 2) | k(2, 3) | v(2, 3) |
    auto cfgs = configs->num_FB_configs;
    auto props = configs->num_properties;
    for (size_t cfg = 0; cfg < cfgs; cfg++) {
      x11::Glx::FbConfig fb_config{};
      bool found = false;
      for (size_t prop = 0; prop < props; prop++) {
        size_t i = 2 * cfg * props + 2 * prop;
        auto key = configs->property_list[i];
        auto value = configs->property_list[i + 1];
        if (key == GLX_VISUAL_ID && value == static_cast<uint32_t>(visual))
          found = true;
        else if (key == GLX_FBCONFIG_ID)
          fb_config = static_cast<x11::Glx::FbConfig>(value);
      }
      if (found)
        return fb_config;
    }
  } else {
    LOG(ERROR) << "GetFBConfigs failed.";
    return {};
  }
  return {};
}

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

}  // namespace

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

GLXFBConfig GetFbConfigForWindow(x11::Connection* connection,
                                 x11::Window window) {
  return GetGlxFbConfigForXProtoFbConfig(
      connection, GetConfigForWindow(connection, window));
}

GLXFBConfig GetGlxFbConfigForXProtoFbConfig(x11::Connection* connection,
                                            x11::Glx::FbConfig xproto_config) {
  if (xproto_config == x11::Glx::FbConfig{})
    return nullptr;
  int attrib_list[] = {GLX_FBCONFIG_ID, static_cast<int>(xproto_config), 0};
  int nitems = 0;
  GLXFBConfig* glx_configs =
      glXChooseFBConfig(connection->GetXlibDisplay(),
                        connection->DefaultScreenId(), attrib_list, &nitems);
  if (!glx_configs)
    return nullptr;
  GLXFBConfig glx_config = glx_configs[0];
  x11::XlibFree(glx_configs);
  return glx_config;
}

}  // namespace gl

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_util.h"

#include <xdg-shell-unstable-v5-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/memory/shared_memory.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"

namespace wl {

namespace {

const uint32_t kShmFormat = WL_SHM_FORMAT_ARGB8888;
const SkColorType kColorType = kBGRA_8888_SkColorType;

uint32_t IdentifyDirectionV5(int hittest) {
  uint32_t direction = 0;
  switch (hittest) {
    case HTBOTTOM:
      direction = xdg_surface_resize_edge::XDG_SURFACE_RESIZE_EDGE_BOTTOM;
      break;
    case HTBOTTOMLEFT:
      direction = xdg_surface_resize_edge::XDG_SURFACE_RESIZE_EDGE_BOTTOM_LEFT;
      break;
    case HTBOTTOMRIGHT:
      direction = xdg_surface_resize_edge::XDG_SURFACE_RESIZE_EDGE_BOTTOM_RIGHT;
      break;
    case HTLEFT:
      direction = xdg_surface_resize_edge::XDG_SURFACE_RESIZE_EDGE_LEFT;
      break;
    case HTRIGHT:
      direction = xdg_surface_resize_edge::XDG_SURFACE_RESIZE_EDGE_RIGHT;
      break;
    case HTTOP:
      direction = xdg_surface_resize_edge::XDG_SURFACE_RESIZE_EDGE_TOP;
      break;
    case HTTOPLEFT:
      direction = xdg_surface_resize_edge::XDG_SURFACE_RESIZE_EDGE_TOP_LEFT;
      break;
    case HTTOPRIGHT:
      direction = xdg_surface_resize_edge::XDG_SURFACE_RESIZE_EDGE_TOP_RIGHT;
      break;
    default:
      direction = xdg_surface_resize_edge::XDG_SURFACE_RESIZE_EDGE_NONE;
      break;
      ;
  }
  return direction;
}

uint32_t IdentifyDirectionV6(int hittest) {
  uint32_t direction = 0;
  switch (hittest) {
    case HTBOTTOM:
      direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM;
      break;
    case HTBOTTOMLEFT:
      direction = zxdg_toplevel_v6_resize_edge::
          ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT;
      break;
    case HTBOTTOMRIGHT:
      direction = zxdg_toplevel_v6_resize_edge::
          ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT;
      break;
    case HTLEFT:
      direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT;
      break;
    case HTRIGHT:
      direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT;
      break;
    case HTTOP:
      direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP;
      break;
    case HTTOPLEFT:
      direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT;
      break;
    case HTTOPRIGHT:
      direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT;
      break;
    default:
      direction =
          zxdg_toplevel_v6_resize_edge::ZXDG_TOPLEVEL_V6_RESIZE_EDGE_NONE;
      break;
  }
  return direction;
}

}  // namespace

wl_buffer* CreateSHMBuffer(const gfx::Size& size,
                           base::SharedMemory* shared_memory,
                           wl_shm* shm) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  int stride = info.minRowBytes();
  size_t image_buffer_size = info.computeByteSize(stride);
  if (image_buffer_size == SIZE_MAX)
    return nullptr;

  if (shared_memory->handle().GetHandle()) {
    shared_memory->Unmap();
    shared_memory->Close();
  }

  if (!shared_memory->CreateAndMapAnonymous(image_buffer_size)) {
    LOG(ERROR) << "Create and mmap failed.";
    return nullptr;
  }

  // TODO(tonikitoo): Use SharedMemory::requested_size instead of
  // 'image_buffer_size'?
  wl::Object<wl_shm_pool> pool;
  pool.reset(wl_shm_create_pool(shm, shared_memory->handle().GetHandle(),
                                image_buffer_size));
  wl_buffer* buffer = wl_shm_pool_create_buffer(
      pool.get(), 0, size.width(), size.height(), stride, kShmFormat);
  return buffer;
}

void DrawBitmapToSHMB(const gfx::Size& size,
                      const base::SharedMemory& shared_memory,
                      const SkBitmap& bitmap) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  int stride = info.minRowBytes();
  sk_sp<SkSurface> sk_surface = SkSurface::MakeRasterDirect(
      SkImageInfo::Make(size.width(), size.height(), kColorType,
                        kOpaque_SkAlphaType),
      static_cast<uint8_t*>(shared_memory.memory()), stride);

  // The |bitmap| contains ARGB image, so update our wl_buffer, which is
  // backed by a SkSurface.
  SkRect damage;
  bitmap.getBounds(&damage);

  // Clear to transparent in case |bitmap| is smaller than the canvas.
  SkCanvas* canvas = sk_surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  canvas->drawBitmapRect(bitmap, damage, nullptr);
}

uint32_t IdentifyDirection(const ui::WaylandConnection& connection,
                           int hittest) {
  if (connection.shell_v6())
    return IdentifyDirectionV6(hittest);
  DCHECK(connection.shell());
  return IdentifyDirectionV5(hittest);
}

}  // namespace wl

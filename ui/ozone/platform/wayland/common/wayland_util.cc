// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/wayland_util.h"

#include <xdg-shell-unstable-v5-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "ui/base/hit_test.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"

namespace wl {

namespace {

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

uint32_t IdentifyDirection(const ui::WaylandConnection& connection,
                           int hittest) {
  if (connection.shell_v6())
    return IdentifyDirectionV6(hittest);
  DCHECK(connection.shell());
  return IdentifyDirectionV5(hittest);
}

bool DrawBitmap(const SkBitmap& bitmap, ui::WaylandShmBuffer* out_buffer) {
  DCHECK(out_buffer);
  DCHECK(out_buffer->GetMemory());
  DCHECK(out_buffer->size() == gfx::Size(bitmap.width(), bitmap.height()));

  auto* mapped_memory = out_buffer->GetMemory();
  auto size = out_buffer->size();
  sk_sp<SkSurface> sk_surface = SkSurface::MakeRasterDirect(
      SkImageInfo::Make(size.width(), size.height(), kColorType,
                        kOpaque_SkAlphaType),
      mapped_memory, out_buffer->stride());

  if (!sk_surface)
    return false;

  // The |bitmap| contains ARGB image, so update our wl_buffer, which is
  // backed by a SkSurface.
  SkRect damage;
  bitmap.getBounds(&damage);

  // Clear to transparent in case |bitmap| is smaller than the canvas.
  auto* canvas = sk_surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  canvas->drawBitmapRect(bitmap, damage, nullptr);
  return true;
}

void ReadDataFromFD(base::ScopedFD fd, std::vector<uint8_t>* contents) {
  DCHECK(contents);
  uint8_t buffer[1 << 10];  // 1 kB in bytes.
  ssize_t length;
  while ((length = read(fd.get(), buffer, sizeof(buffer))) > 0)
    contents->insert(contents->end(), buffer, buffer + length);
}

}  // namespace wl

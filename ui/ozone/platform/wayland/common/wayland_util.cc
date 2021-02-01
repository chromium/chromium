// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/wayland_util.h"

#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace wl {

namespace {

const SkColorType kColorType = kBGRA_8888_SkColorType;

uint32_t IdentifyDirectionStable(int hittest) {
  uint32_t direction = 0;
  switch (hittest) {
    case HTBOTTOM:
      direction = xdg_toplevel_resize_edge::XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
      break;
    case HTBOTTOMLEFT:
      direction =
          xdg_toplevel_resize_edge::XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
      break;
    case HTBOTTOMRIGHT:
      direction =
          xdg_toplevel_resize_edge::XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
      break;
    case HTLEFT:
      direction = xdg_toplevel_resize_edge::XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
      break;
    case HTRIGHT:
      direction = xdg_toplevel_resize_edge::XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
      break;
    case HTTOP:
      direction = xdg_toplevel_resize_edge::XDG_TOPLEVEL_RESIZE_EDGE_TOP;
      break;
    case HTTOPLEFT:
      direction = xdg_toplevel_resize_edge::XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
      break;
    case HTTOPRIGHT:
      direction = xdg_toplevel_resize_edge::XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
      break;
    default:
      direction = xdg_toplevel_resize_edge::XDG_TOPLEVEL_RESIZE_EDGE_NONE;
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
  if (connection.shell())
    return IdentifyDirectionStable(hittest);
  DCHECK(connection.shell_v6());
  return IdentifyDirectionV6(hittest);
}

bool DrawBitmap(const SkBitmap& bitmap, ui::WaylandShmBuffer* out_buffer) {
  DCHECK(out_buffer);
  DCHECK(out_buffer->GetMemory());
  DCHECK_EQ(out_buffer->size(), gfx::Size(bitmap.width(), bitmap.height()));

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
  canvas->drawImageRect(bitmap.asImage(), damage, SkSamplingOptions());
  return true;
}

void ReadDataFromFD(base::ScopedFD fd, std::vector<uint8_t>* contents) {
  DCHECK(contents);
  uint8_t buffer[1 << 10];  // 1 kB in bytes.
  ssize_t length;
  while ((length = read(fd.get(), buffer, sizeof(buffer))) > 0)
    contents->insert(contents->end(), buffer, buffer + length);
}

gfx::Rect TranslateBoundsToParentCoordinates(const gfx::Rect& child_bounds,
                                             const gfx::Rect& parent_bounds) {
  return gfx::Rect(
      (child_bounds.origin() - parent_bounds.origin().OffsetFromOrigin()),
      child_bounds.size());
}

gfx::Rect TranslateBoundsToTopLevelCoordinates(const gfx::Rect& child_bounds,
                                               const gfx::Rect& parent_bounds) {
  return gfx::Rect(
      (child_bounds.origin() + parent_bounds.origin().OffsetFromOrigin()),
      child_bounds.size());
}

wl_output_transform ToWaylandTransform(gfx::OverlayTransform transform) {
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      return WL_OUTPUT_TRANSFORM_NORMAL;
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return WL_OUTPUT_TRANSFORM_FLIPPED;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return WL_OUTPUT_TRANSFORM_FLIPPED_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return WL_OUTPUT_TRANSFORM_90;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return WL_OUTPUT_TRANSFORM_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return WL_OUTPUT_TRANSFORM_270;
    default:
      break;
  }
  NOTREACHED();
  return WL_OUTPUT_TRANSFORM_NORMAL;
}

gfx::Rect ApplyWaylandTransform(const gfx::Rect& rect,
                                const gfx::Size& bounds,
                                wl_output_transform transform) {
  gfx::Rect result = rect;
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED:
      result.set_x(bounds.width() - rect.x() - rect.width());
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
      result.set_x(rect.y());
      result.set_y(rect.x());
      result.set_width(rect.height());
      result.set_height(rect.width());
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
      result.set_y(bounds.height() - rect.y() - rect.height());
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      result.set_x(bounds.height() - rect.y() - rect.height());
      result.set_y(bounds.width() - rect.x() - rect.width());
      result.set_width(rect.height());
      result.set_height(rect.width());
      break;
    case WL_OUTPUT_TRANSFORM_90:
      result.set_x(rect.y());
      result.set_y(bounds.width() - rect.x() - rect.width());
      result.set_width(rect.height());
      result.set_height(rect.width());
      break;
    case WL_OUTPUT_TRANSFORM_180:
      result.set_x(bounds.width() - rect.x() - rect.width());
      result.set_y(bounds.height() - rect.y() - rect.height());
      break;
    case WL_OUTPUT_TRANSFORM_270:
      result.set_x(bounds.height() - rect.y() - rect.height());
      result.set_y(rect.x());
      result.set_width(rect.height());
      result.set_height(rect.width());
      break;
    default:
      NOTREACHED();
      break;
  }
  return result;
}

gfx::Size ApplyWaylandTransform(const gfx::Size& size,
                                wl_output_transform transform) {
  gfx::Size result = size;
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_FLIPPED:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
    case WL_OUTPUT_TRANSFORM_180:
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
      result.set_width(size.height());
      result.set_height(size.width());
      break;
    default:
      NOTREACHED();
      break;
  }
  return result;
}

bool IsMenuType(ui::PlatformWindowType type) {
  return type == ui::PlatformWindowType::kMenu ||
         type == ui::PlatformWindowType::kPopup;
}

ui::WaylandWindow* RootWindowFromWlSurface(wl_surface* surface) {
  if (!surface)
    return nullptr;
  auto* wayland_surface = static_cast<ui::WaylandSurface*>(
      wl_proxy_get_user_data(reinterpret_cast<wl_proxy*>(surface)));
  if (!wayland_surface)
    return nullptr;
  return wayland_surface->root_window();
}

gfx::Rect TranslateWindowBoundsToParentDIP(ui::WaylandWindow* window,
                                           ui::WaylandWindow* parent_window) {
  DCHECK(window);
  DCHECK(parent_window);
  DCHECK_EQ(window->buffer_scale(), parent_window->buffer_scale());
  DCHECK_EQ(window->ui_scale(), parent_window->ui_scale());
  return gfx::ScaleToRoundedRect(
      wl::TranslateBoundsToParentCoordinates(window->GetBounds(),
                                             parent_window->GetBounds()),
      1.0 / window->buffer_scale());
}

std::vector<gfx::Rect> CreateRectsFromSkPath(const SkPath& path) {
  SkRegion clip_region;
  clip_region.setRect(path.getBounds().round());
  SkRegion region;
  region.setPath(path, clip_region);

  std::vector<gfx::Rect> rects;
  for (SkRegion::Iterator it(region); !it.done(); it.next())
    rects.push_back(gfx::SkIRectToRect(it.rect()));

  return rects;
}

SkPath ConvertPathToDIP(const SkPath& path_in_pixels, const int32_t scale) {
  SkScalar sk_scale = SkFloatToScalar(1.0f / scale);
  gfx::Transform transform;
  transform.Scale(sk_scale, sk_scale);
  SkPath path_in_dips;
  path_in_pixels.transform(SkMatrix(transform.matrix()), &path_in_dips);
  return path_in_dips;
}

}  // namespace wl

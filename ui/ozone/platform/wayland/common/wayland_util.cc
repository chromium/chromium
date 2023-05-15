// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/wayland_util.h"

#include <xdg-shell-client-protocol.h>

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace wl {

namespace {

const SkColorType kColorType = kBGRA_8888_SkColorType;

}  // namespace

uint32_t IdentifyDirection(int hittest) {
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

bool DrawBitmap(const SkBitmap& bitmap, ui::WaylandShmBuffer* out_buffer) {
  DCHECK(out_buffer);
  DCHECK(out_buffer->GetMemory());
  DCHECK(gfx::Rect(out_buffer->size())
             .Contains(gfx::Rect(bitmap.width(), bitmap.height())));

  auto* mapped_memory = out_buffer->GetMemory();
  auto size = out_buffer->size();
  sk_sp<SkSurface> sk_surface =
      SkSurfaces::WrapPixels(SkImageInfo::Make(size.width(), size.height(),
                                               kColorType, kOpaque_SkAlphaType),
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

gfx::RectF TranslateBoundsToParentCoordinatesF(
    const gfx::RectF& child_bounds,
    const gfx::RectF& parent_bounds) {
  return gfx::RectF(
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
    // gfx::OverlayTransform and Wayland buffer transforms rotate in opposite
    // directions relative to each other, so swap 90 and 270.
    // TODO(rivr): Currently all wl_buffers are created without y inverted, so
    // this may need to be revisited if that changes.
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return WL_OUTPUT_TRANSFORM_270;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return WL_OUTPUT_TRANSFORM_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return WL_OUTPUT_TRANSFORM_90;
    default:
      break;
  }
  NOTREACHED();
  return WL_OUTPUT_TRANSFORM_NORMAL;
}

gfx::RectF ApplyWaylandTransform(const gfx::RectF& rect,
                                 const gfx::SizeF& bounds,
                                 wl_output_transform transform) {
  gfx::RectF result = rect;
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

gfx::SizeF ApplyWaylandTransform(const gfx::SizeF& size,
                                 wl_output_transform transform) {
  gfx::SizeF result = size;
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
  DCHECK_EQ(window->applied_state().window_scale,
            parent_window->applied_state().window_scale);
  DCHECK_EQ(window->ui_scale(), parent_window->ui_scale());
  return wl::TranslateBoundsToParentCoordinates(
      window->GetBoundsInDIP(), parent_window->GetBoundsInDIP());
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

SkPath ConvertPathToDIP(const SkPath& path_in_pixels, float scale) {
  SkScalar sk_scale = SkFloatToScalar(1.0f / scale);
  SkPath path_in_dips;
  path_in_pixels.transform(SkMatrix::Scale(sk_scale, sk_scale), &path_in_dips);
  return path_in_dips;
}

void SkColorToWlArray(const SkColor& color, wl_array& array) {
  SkColor4f precise_color = SkColor4f::FromColor(color);
  SkColorToWlArray(precise_color, array);
}

void SkColorToWlArray(const SkColor4f& color, wl_array& array) {
  for (float component : color.array()) {
    float* ptr = static_cast<float*>(wl_array_add(&array, sizeof(float)));
    DCHECK(ptr);
    *ptr = component;
  }
}

}  // namespace wl

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/wayland/common/wayland_util.h"

#include <sys/socket.h>
#include <xdg-shell-client-protocol.h>

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "build/buildflag.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "ui/events/base_event_utils.h"
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
  std::array<uint8_t, 1 << 10> buffer;  // 1 kB in bytes.
  ssize_t length;
  while ((length = read(fd.get(), buffer.data(), buffer.size())) > 0) {
    contents->insert(contents->end(), buffer.begin(), buffer.begin() + length);
  }
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
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
      return WL_OUTPUT_TRANSFORM_270;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
      return WL_OUTPUT_TRANSFORM_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
      return WL_OUTPUT_TRANSFORM_90;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270:
    default:
      break;
  }
  NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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

void TransformToWlArray(
    const absl::variant<gfx::OverlayTransform, gfx::Transform>& transform,
    wl_array& array) {
  if (absl::holds_alternative<gfx::OverlayTransform>(transform)) {
    return;
  }

  gfx::Transform t = absl::get<gfx::Transform>(transform);
  constexpr std::array<std::array<int, 2>, 6> rcs = {
      {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {0, 3}, {1, 3}}};

  for (const auto& rc : rcs) {
    float* ptr = static_cast<float*>(wl_array_add(&array, sizeof(float)));
    DCHECK(ptr);
    *ptr = static_cast<float>(t.rc(rc[0], rc[1]));
  }
}

base::TimeTicks EventMillisecondsToTimeTicks(uint32_t milliseconds) {
#if BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/40287874): `milliseconds` comes from Weston that
  // uses timestamp from libinput, which is different from TimeTicks.
  // Use EventTimeForNow(), for now.
  return ui::EventTimeForNow();
#else
  return base::TimeTicks() + base::Milliseconds(milliseconds);
#endif
}

float ClampScale(float scale) {
  return std::max(1.f, scale);
}

bool MaybeHandlePlatformEventForDrag(const ui::PlatformEvent& event,
                                     bool start_drag_ack_received,
                                     base::OnceClosure cancel_drag_cb) {
  // Two distinct problematic edge cases are handled here, where mouse button or
  // touch release events come in after start_drag has already been requested:
  //
  // 1. If it's received before the drag session effectively starts at
  //    compositor side, which is possible given the asynchronous nature of the
  //    Wayland protocol. In this case, to preventing UI from getting stuck on
  //    the drag nested loop, we just abort the drag session.
  //
  // 2. Otherwise, button release events may be received from buggy compositors
  //    in addition to the actual dnd drop events, in which case the event is
  //    suppressed, otherwise it leads to broken UI state, as observed for
  //    example in https://crbug.com/329703410.
  if (!event->IsSynthesized() &&
      (event->type() == ui::EventType::kMouseReleased ||
       event->type() == ui::EventType::kTouchReleased)) {
    if (!start_drag_ack_received) {
      std::move(cancel_drag_cb).Run();
    } else {
      return true;
    }
  }
  return false;
}

void RecordConnectionMetrics(wl_display* display) {
#if BUILDFLAG(IS_LINUX)
  CHECK(display);

  // These values are logged to metrics so must not be changed.
  enum class WaylandCompositor {
    // Couldn't obtain compositor name.
    kUnknown = 0,
    // Obtained compositor name, but don't have an enum value for it.
    kOther = 1,

    kAnvil = 2,
    kCage = 3,
    kCosmic = 4,
    kDwl = 5,
    kGamescope = 6,
    kHyprland = 7,
    kKWin = 8,
    kLabwc = 9,
    kMiracle = 10,
    kMutter = 11,
    kNiri = 12,
    kQtile = 13,
    kRiver = 14,
    kSway = 15,
    kTheseus = 16,
    kWayfire = 17,
    kWeston = 18,

    kMaxValue = kWeston,
  };

  auto get_compositor = [&]() {
    struct {
      const char* name;
      WaylandCompositor compositor;
    } constexpr kCompositors[] = {
        {"anvil", WaylandCompositor::kAnvil},
        {"cage", WaylandCompositor::kCage},
        {"cosmic", WaylandCompositor::kCosmic},
        {"dwl", WaylandCompositor::kDwl},
        {"gamescope", WaylandCompositor::kGamescope},
        {"hyprland", WaylandCompositor::kHyprland},
        {"kwin", WaylandCompositor::kKWin},
        {"labwc", WaylandCompositor::kLabwc},
        {"miracle", WaylandCompositor::kMiracle},
        {"mutter", WaylandCompositor::kMutter},
        {"niri", WaylandCompositor::kNiri},
        {"qtile", WaylandCompositor::kQtile},
        {"river", WaylandCompositor::kRiver},
        {"sway", WaylandCompositor::kSway},
        {"theseus", WaylandCompositor::kTheseus},
        {"wayfire", WaylandCompositor::kWayfire},
        {"weston", WaylandCompositor::kWeston},
    };

    const int fd = wl_display_get_fd(display);
    if (fd == -1) {
      return WaylandCompositor::kUnknown;
    }

    ucred credentials{.pid = 0};
    socklen_t size = sizeof(ucred);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &size) == -1) {
      return WaylandCompositor::kUnknown;
    }

    std::string name;
    if (!base::ReadFileToStringNonBlocking(
            base::FilePath(
                base::StringPrintf("/proc/%d/comm", credentials.pid)),
            &name)) {
      return WaylandCompositor::kUnknown;
    }

    for (const auto& [name_key, compositor] : kCompositors) {
      if (base::StartsWith(name, name_key,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        return compositor;
      }
    }

    return WaylandCompositor::kOther;
  };

  base::UmaHistogramEnumeration("Linux.Wayland.Compositor", get_compositor());
#endif
}

}  // namespace wl

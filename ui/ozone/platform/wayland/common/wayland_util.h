// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_UTIL_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_UTIL_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/platform_window/platform_window_init_properties.h"

class SkBitmap;
class SkPath;

namespace ui {
class WaylandConnection;
class WaylandShmBuffer;
class WaylandWindow;
}  // namespace ui

namespace gfx {
enum class BufferFormat;
class Size;
}  // namespace gfx

namespace wl {

using RequestSizeCallback = base::OnceCallback<void(const gfx::Size&)>;

using OnRequestBufferCallback =
    base::OnceCallback<void(wl::Object<struct wl_buffer>)>;

using BufferFormatsWithModifiersMap =
    base::flat_map<gfx::BufferFormat, std::vector<uint64_t>>;

// Identifies the direction of the "hittest" for Wayland. |connection|
// is used to identify whether values from shell v5 or v6 must be used.
uint32_t IdentifyDirection(const ui::WaylandConnection& connection,
                           int hittest);

// Draws |bitmap| into |out_buffer|. Returns if no errors occur, and false
// otherwise. It assumes the bitmap fits into the buffer and buffer is
// currently mmap'ed in memory address space.
bool DrawBitmap(const SkBitmap& bitmap, ui::WaylandShmBuffer* out_buffer);

// Helper function to read data from a file.
void ReadDataFromFD(base::ScopedFD fd, std::vector<uint8_t>* contents);

// Translates bounds relative to top level window to specified parent.
gfx::Rect TranslateBoundsToParentCoordinates(const gfx::Rect& child_bounds,
                                             const gfx::Rect& parent_bounds);
// Translates bounds relative to parent window to top level window.
gfx::Rect TranslateBoundsToTopLevelCoordinates(const gfx::Rect& child_bounds,
                                               const gfx::Rect& parent_bounds);

// Returns wl_output_transform corresponding |transform|. |transform| is an
// enumeration of a fixed selection of transformations.
wl_output_transform ToWaylandTransform(gfx::OverlayTransform transform);

// |bounds| contains |rect|. ApplyWaylandTransform() returns the resulted
// |rect| after transformation is applied to |bounds| containing |rect| as a
// whole.
gfx::Rect ApplyWaylandTransform(const gfx::Rect& rect,
                                const gfx::Size& bounds,
                                wl_output_transform transform);

// Applies transformation to |size|.
gfx::Size ApplyWaylandTransform(const gfx::Size& size,
                                wl_output_transform transform);

// Says if the type is kPopup or kMenu.
bool IsMenuType(ui::PlatformWindowType type);

// Returns the root WaylandWindow for the given wl_surface.
ui::WaylandWindow* RootWindowFromWlSurface(wl_surface* surface);

// Returns bounds of the given window, adjusted to its subsurface. We need to
// adjust bounds because WaylandWindow::GetBounds() returns absolute bounds in
// pixels, but wl_subsurface works with bounds relative to the parent surface
// and in DIP.
gfx::Rect TranslateWindowBoundsToParentDIP(ui::WaylandWindow* window,
                                           ui::WaylandWindow* parent_window);

// Returns rectangles dictated by SkPath.
std::vector<gfx::Rect> CreateRectsFromSkPath(const SkPath& path);

// Returns converted SkPath in DIPs from the one in pixels.
SkPath ConvertPathToDIP(const SkPath& path_in_pixels, const int32_t scale);

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_UTIL_H_

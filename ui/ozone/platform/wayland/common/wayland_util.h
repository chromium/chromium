// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_UTIL_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_UTIL_H_

#include <vector>

#include <wayland-client.h>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

class SkBitmap;

namespace ui {
class WaylandConnection;
class WaylandShmBuffer;
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

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_UTIL_H_

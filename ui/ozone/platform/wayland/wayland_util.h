// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTIL_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTIL_H_

#include <wayland-client.h>

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/ozone/platform/wayland/wayland_object.h"

class SkBitmap;

namespace base {
class SharedMemory;
}

namespace ui {
class WaylandConnection;
}

namespace gfx {
class Size;
enum class SwapResult;
struct PresentationFeedback;
}

namespace wl {

// Corresponds to mojom::WaylandConnection::ScheduleBufferSwapCallback.
using BufferSwapCallback =
    base::OnceCallback<void(gfx::SwapResult, const gfx::PresentationFeedback&)>;

wl_buffer* CreateSHMBuffer(const gfx::Size& size,
                           base::SharedMemory* shared_memory,
                           wl_shm* shm);
void DrawBitmapToSHMB(const gfx::Size& size,
                      const base::SharedMemory& shared_memory,
                      const SkBitmap& bitmap);

// Identifies the direction of the "hittest" for Wayland. |connection|
// is used to identify whether values from shell v5 or v6 must be used.
uint32_t IdentifyDirection(const ui::WaylandConnection& connection,
                           int hittest);

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTIL_H_

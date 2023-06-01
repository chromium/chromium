// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"

class SkBitmap;

struct wl_cursor;

namespace gfx {
class Point;
}

namespace ui {

class WaylandConnection;
class WaylandPointer;

// Interface through which WaylandCursor notifies the listener that it has
// attached another buffer to the pointer surface.  The listener may free the
// previous buffer if it was holding it.
class WaylandCursorBufferListener {
 public:
  // Tells the listener that a new buffer is attached.  |cursor_data| may be
  // non-nullptr if the platform shape is used, or nullptr if the cursor has
  // been hidden, or a custom bitmap has been set.
  virtual void OnCursorBufferAttached(wl_cursor* cursor_data) = 0;

 protected:
  virtual ~WaylandCursorBufferListener() = default;
};

// Manages the actual visual representation (what users see drawn) of the
// 'pointer' (which is the Wayland term for mouse/mice).
//
// Encapsulates the low-level job such as surface and buffer management and
// Wayland protocol calls.
class WaylandCursor {
 public:
  WaylandCursor(WaylandPointer* pointer, WaylandConnection* connection);

  WaylandCursor(const WaylandCursor&) = delete;
  WaylandCursor& operator=(const WaylandCursor&) = delete;

  ~WaylandCursor();

  // Updates wl_pointer's visual representation with the given bitmap
  // image set and hotspot.
  // The cursor bitmap should already match the device scale factor.
  // `buffer_scale` should match the scale of the window surface, so that we
  // can pass this information to the compositor which will avoid scaling it
  // again.
  void UpdateBitmap(const std::vector<SkBitmap>& bitmaps,
                    const gfx::Point& hotspot_in_dips,
                    int buffer_scale);

  // Takes data managed by the platform (without taking ownership).
  void SetPlatformShape(wl_cursor* cursor_data,
                        int buffer_scale);

  void set_listener(WaylandCursorBufferListener* listener) {
    listener_ = listener;
  }

 private:
  // wl_buffer_listener:
  static void OnBufferRelease(void* data, wl_buffer* wl_buffer);

  void HideCursor();

  // Prepares the platform cursor data for use.  Starts animation if needed.
  void SetPlatformShapeInternal();

  // Does all Wayland-level calls necessary to update the cursor shape.
  void AttachAndCommit(wl_buffer* buffer,
                       uint32_t buffer_width,
                       uint32_t buffer_height,
                       uint32_t hotspot_x_dip,
                       uint32_t hotspot_y_dip);

  const raw_ptr<WaylandPointer, DanglingUntriaged> pointer_;
  const raw_ptr<WaylandConnection> connection_;
  const wl::Object<wl_surface> pointer_surface_;

  // Holds the buffers and their memory until the compositor releases them.
  base::flat_map<wl_buffer*, WaylandShmBuffer> buffers_;
  raw_ptr<WaylandCursorBufferListener> listener_ = nullptr;

  // Current platform cursor.
  raw_ptr<wl_cursor> cursor_data_ = nullptr;
  size_t current_image_index_ = 0;
  int buffer_scale_ = 1;
  base::RepeatingTimer animation_timer_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_CURSOR_H_

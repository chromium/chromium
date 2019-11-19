// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POINTER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POINTER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor.h"

namespace ui {

class WaylandWindow;

// Wraps the wl_pointer object and transmits events to the dispatcher callback.
//
// Exposes an aggregated WaylandCursor that manages the visual shape of the
// pointer.
class WaylandPointer {
 public:
  WaylandPointer(wl_pointer* pointer, const EventDispatchCallback& callback);
  virtual ~WaylandPointer();

  void set_connection(WaylandConnection* connection) {
    connection_ = connection;
    cursor_->Init(obj_.get(), connection_);
  }

  int GetFlagsWithKeyboardModifiers();
  void ResetFlags();

  WaylandCursor* cursor() { return cursor_.get(); }

  void reset_window_with_pointer_focus() {
    window_with_pointer_focus_ = nullptr;
  }

 private:
  // wl_pointer_listener
  static void Enter(void* data,
                    wl_pointer* obj,
                    uint32_t serial,
                    wl_surface* surface,
                    wl_fixed_t surface_x,
                    wl_fixed_t surface_y);
  static void Leave(void* data,
                    wl_pointer* obj,
                    uint32_t serial,
                    wl_surface* surface);
  static void Motion(void* data,
                     wl_pointer* obj,
                     uint32_t time,
                     wl_fixed_t surface_x,
                     wl_fixed_t surface_y);
  static void Button(void* data,
                     wl_pointer* obj,
                     uint32_t serial,
                     uint32_t time,
                     uint32_t button,
                     uint32_t state);
  static void Axis(void* data,
                   wl_pointer* obj,
                   uint32_t time,
                   uint32_t axis,
                   wl_fixed_t value);

  void MaybeSetOrResetImplicitGrab();
  void FocusWindow(wl_surface* surface);
  void UnfocusWindow(wl_surface* surface);

  WaylandConnection* connection_ = nullptr;
  std::unique_ptr<WaylandCursor> cursor_;
  wl::Object<wl_pointer> obj_;
  EventDispatchCallback callback_;
  gfx::PointF location_;
  // Flags is a bitmask of EventFlags corresponding to the pointer/keyboard
  // state.
  int flags_ = 0;

  // Keeps track of current modifiers. These are needed in order to properly
  // update |flags_| with newest modifiers.
  int keyboard_modifiers_ = 0;

  // The window the mouse is over.
  WaylandWindow* window_with_pointer_focus_ = nullptr;

  base::WeakPtrFactory<WaylandPointer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPointer);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POINTER_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POINTER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POINTER_H_

#include "base/macros.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class PointF;
class Vector2d;
}  // namespace gfx

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Wraps the wl_pointer object and injects event data through
// |WaylandPointer::Delegate| interface.
class WaylandPointer {
 public:
  class Delegate;

  WaylandPointer(wl_pointer* pointer,
                 WaylandConnection* connection,
                 Delegate* delegate);
  virtual ~WaylandPointer();

  wl_pointer* wl_object() const { return obj_.get(); }

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
  static void Frame(void* data, wl_pointer* obj);
  static void AxisSource(void* data, wl_pointer* obj, uint32_t axis_source);
  static void AxisStop(void* data,
                       wl_pointer* obj,
                       uint32_t time,
                       uint32_t axis);
  static void AxisDiscrete(void* data,
                           wl_pointer* obj,
                           uint32_t axis,
                           int32_t discrete);

  wl::Object<wl_pointer> obj_;
  WaylandConnection* const connection_;
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPointer);
};

class WaylandPointer::Delegate {
 public:
  virtual void OnPointerCreated(WaylandPointer* pointer) = 0;
  virtual void OnPointerDestroyed(WaylandPointer* pointer) = 0;
  virtual void OnPointerFocusChanged(WaylandWindow* window,
                                     const gfx::PointF& location) = 0;
  virtual void OnPointerButtonEvent(EventType evtype,
                                    int changed_button,
                                    WaylandWindow* window = nullptr) = 0;
  virtual void OnPointerMotionEvent(const gfx::PointF& location) = 0;
  virtual void OnPointerAxisEvent(const gfx::Vector2d& offset) = 0;
  virtual void OnPointerFrameEvent() = 0;
  virtual void OnPointerAxisSourceEvent(uint32_t axis_source) = 0;
  virtual void OnPointerAxisStopEvent(uint32_t axis) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POINTER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/events/pointer_details.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {

class WaylandConnection;
class WaylandWindow;

class WaylandTouch {
 public:
  class Delegate;

  WaylandTouch(wl_touch* touch,
               WaylandConnection* connection,
               Delegate* delegate);
  ~WaylandTouch();

 private:
  // wl_touch_listener
  static void Down(void* data,
                   wl_touch* obj,
                   uint32_t serial,
                   uint32_t time,
                   struct wl_surface* surface,
                   int32_t id,
                   wl_fixed_t x,
                   wl_fixed_t y);
  static void Up(void* data,
                 wl_touch* obj,
                 uint32_t serial,
                 uint32_t time,
                 int32_t id);
  static void Motion(void* data,
                     wl_touch* obj,
                     uint32_t time,
                     int32_t id,
                     wl_fixed_t x,
                     wl_fixed_t y);
  static void Cancel(void* data, wl_touch* obj);
  static void Frame(void* data, wl_touch* obj);

  wl::Object<wl_touch> obj_;
  WaylandConnection* const connection_;
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(WaylandTouch);
};

class WaylandTouch::Delegate {
 public:
  virtual void OnTouchPressEvent(WaylandWindow* window,
                                 const gfx::PointF& location,
                                 base::TimeTicks timestamp,
                                 PointerId id) = 0;
  virtual void OnTouchReleaseEvent(base::TimeTicks timestamp, PointerId id) = 0;
  virtual void OnTouchMotionEvent(const gfx::PointF& location,
                                  base::TimeTicks timestamp,
                                  PointerId id) = 0;
  virtual void OnTouchCancelEvent() = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_

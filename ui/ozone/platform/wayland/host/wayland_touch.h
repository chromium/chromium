// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

class WaylandTouch : public WaylandWindowObserver {
 public:
  WaylandTouch(wl_touch* touch, const EventDispatchCallback& callback);
  ~WaylandTouch() override;

  void SetConnection(WaylandConnection* connection);

  void RemoveTouchPoints(const WaylandWindow* window);

 private:
  struct TouchPoint {
    TouchPoint();
    TouchPoint(gfx::Point location, wl_surface* current_surface);
    ~TouchPoint();

    wl_surface* surface = nullptr;
    gfx::Point last_known_location;
  };

  using TouchPoints = base::flat_map<int32_t, TouchPoint>;

  void MaybeUnsetFocus(const TouchPoints& points,
                       int32_t id,
                       wl_surface* surface);

  // WaylandWindowObserver implements:
  void OnWindowRemoved(WaylandWindow* window) override;

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
  static void Frame(void* data, wl_touch* obj);
  static void Cancel(void* data, wl_touch* obj);

  WaylandConnection* connection_ = nullptr;
  wl::Object<wl_touch> obj_;
  EventDispatchCallback callback_;
  TouchPoints current_points_;

  DISALLOW_COPY_AND_ASSIGN(WaylandTouch);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_

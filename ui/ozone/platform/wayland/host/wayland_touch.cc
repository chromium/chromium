// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_touch.h"

#include <sys/mman.h>
#include <wayland-client.h>

#include "base/files/scoped_file.h"
#include "ui/base/buildflags.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {

WaylandTouch::TouchPoint::TouchPoint() = default;

WaylandTouch::TouchPoint::TouchPoint(gfx::Point location,
                                     wl_surface* current_surface)
    : surface(current_surface), last_known_location(location) {}

WaylandTouch::TouchPoint::~TouchPoint() = default;

//-----------------------------------------------------------------------------

WaylandTouch::WaylandTouch(wl_touch* touch,
                           const EventDispatchCallback& callback)
    : obj_(touch), callback_(callback) {
  static const wl_touch_listener listener = {
      &WaylandTouch::Down,  &WaylandTouch::Up,     &WaylandTouch::Motion,
      &WaylandTouch::Frame, &WaylandTouch::Cancel,
  };

  wl_touch_add_listener(obj_.get(), &listener, this);
}

WaylandTouch::~WaylandTouch() {
  DCHECK(current_points_.empty());
}

void WaylandTouch::SetConnection(WaylandConnection* connection) {
  connection_ = connection;

  // Observs remove changes to know when touch points can be removed.
  connection_->wayland_window_manager()->AddObserver(this);
}

void WaylandTouch::RemoveTouchPoints(const WaylandWindow* window) {
  base::EraseIf(current_points_,
                [window](const TouchPoints::value_type& point) {
                  return point.second.surface == window->surface();
                });
}

void WaylandTouch::MaybeUnsetFocus(const WaylandTouch::TouchPoints& points,
                                   int32_t id,
                                   wl_surface* surface) {
  for (const auto& point : points) {
    // Return early on the first other point having this surface.
    if (surface == point.second.surface && id != point.first)
      return;
  }
  DCHECK(surface);
  WaylandWindow::FromSurface(surface)->set_touch_focus(false);
}

void WaylandTouch::OnWindowRemoved(WaylandWindow* window) {
  RemoveTouchPoints(window);
}

void WaylandTouch::Down(void* data,
                        wl_touch* obj,
                        uint32_t serial,
                        uint32_t time,
                        struct wl_surface* surface,
                        int32_t id,
                        wl_fixed_t x,
                        wl_fixed_t y) {
  if (!surface)
    return;
  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);
  touch->connection_->set_serial(serial);
  WaylandWindow::FromSurface(surface)->set_touch_focus(true);

  // Make sure this touch point wasn't present before.
  if (touch->current_points_.find(id) != touch->current_points_.end()) {
    LOG(WARNING) << "Touch down fired with wrong id";
    return;
  }

  EventType type = ET_TOUCH_PRESSED;
  gfx::Point location(wl_fixed_to_double(x), wl_fixed_to_double(y));
  base::TimeTicks time_stamp =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(time);
  PointerDetails pointer_details(EventPointerType::POINTER_TYPE_TOUCH, id);
  TouchEvent event(type, location, time_stamp, pointer_details);
  touch->callback_.Run(&event);

  touch->current_points_[id] = TouchPoint(location, surface);
}

void WaylandTouch::Up(void* data,
                      wl_touch* obj,
                      uint32_t serial,
                      uint32_t time,
                      int32_t id) {
  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);
  const auto iterator = touch->current_points_.find(id);

  // Make sure this touch point was present before.
  if (iterator == touch->current_points_.end()) {
    LOG(WARNING) << "Touch up fired with no matching touch down";
    return;
  }

  EventType type = ET_TOUCH_RELEASED;
  base::TimeTicks time_stamp =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(time);
  PointerDetails pointer_details(EventPointerType::POINTER_TYPE_TOUCH, id);
  TouchEvent event(type, touch->current_points_[id].last_known_location,
                   time_stamp, pointer_details);
  touch->callback_.Run(&event);

  touch->MaybeUnsetFocus(touch->current_points_, id,
                         touch->current_points_[id].surface);
  touch->current_points_.erase(iterator);
}

void WaylandTouch::Motion(void* data,
                          wl_touch* obj,
                          uint32_t time,
                          int32_t id,
                          wl_fixed_t x,
                          wl_fixed_t y) {
  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);

  // Make sure this touch point wasn't present before.
  if (touch->current_points_.find(id) == touch->current_points_.end()) {
    LOG(WARNING) << "Touch event fired with wrong id";
    return;
  }

  EventType type = ET_TOUCH_MOVED;
  gfx::Point location(wl_fixed_to_double(x), wl_fixed_to_double(y));
  base::TimeTicks time_stamp =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(time);
  PointerDetails pointer_details(EventPointerType::POINTER_TYPE_TOUCH, id);
  TouchEvent event(type, location, time_stamp, pointer_details);
  touch->callback_.Run(&event);
  touch->current_points_[id].last_known_location = location;
}

void WaylandTouch::Frame(void* data, wl_touch* obj) {}

void WaylandTouch::Cancel(void* data, wl_touch* obj) {
  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);
  for (auto& point : touch->current_points_) {
    int32_t id = point.first;

    EventType type = ET_TOUCH_CANCELLED;
    base::TimeTicks time_stamp = base::TimeTicks::Now();
    PointerDetails pointer_details(EventPointerType::POINTER_TYPE_TOUCH, id);
    TouchEvent event(type, gfx::Point(), time_stamp, pointer_details);
    touch->callback_.Run(&event);

    WaylandWindow::FromSurface(point.second.surface)->set_touch_focus(false);
  }
  touch->current_points_.clear();
}

}  // namespace ui

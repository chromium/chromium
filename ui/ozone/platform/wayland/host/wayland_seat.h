// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SEAT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SEAT_H_

#include <cstdint>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;
class WaylandKeyboard;
class WaylandPointer;
class WaylandTouch;

// Wraps the Wayland seat abstraction.
// See https://wayland-book.com/seat.html
class WaylandSeat : public wl::GlobalObjectRegistrar<WaylandSeat> {
 public:
  static constexpr char kInterfaceName[] = "wl_seat";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandSeat(wl_seat* seat, WaylandConnection* connection);
  WaylandSeat(const WaylandSeat&) = delete;
  WaylandSeat& operator=(const WaylandSeat&) = delete;
  ~WaylandSeat();

  wl_seat* wl_object() const { return obj_.get(); }

  // Returns the current touch, which may be null.
  WaylandTouch* touch() const { return touch_.get(); }

  // Returns the current pointer, which may be null.
  WaylandPointer* pointer() const { return pointer_.get(); }

  // Returns the current keyboard, which may be null.
  WaylandKeyboard* keyboard() const { return keyboard_.get(); }

  // Creates or re-creates the keyboard object with the currently acquired
  // protocol objects, if possible.
  // Returns whether the object was created.
  bool RefreshKeyboard();

 private:
  // wl_seat_listener callbacks:
  static void OnCapabilities(void* data, wl_seat* seat, uint32_t capabilities);
  static void OnName(void* data, wl_seat* seat, const char* name);

  void HandleCapabilities(void* data, wl_seat* seat, uint32_t capabilities);

  // Wayland object wrapped by this class.
  wl::Object<wl_seat> obj_;

  const raw_ptr<WaylandConnection> connection_;

  // Input device objects.
  std::unique_ptr<WaylandKeyboard> keyboard_;
  std::unique_ptr<WaylandPointer> pointer_;
  std::unique_ptr<WaylandTouch> touch_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SEAT_H_

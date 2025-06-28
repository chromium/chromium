// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_H_

#include <cstdint>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandTabletSeat;

// Wraps the zwp_tablet_v2 object.
class WaylandTablet {
 public:
  WaylandTablet(zwp_tablet_v2* tablet, WaylandTabletSeat* seat);
  WaylandTablet(const WaylandTablet&) = delete;
  WaylandTablet& operator=(const WaylandTablet&) = delete;
  ~WaylandTablet();

  uint32_t id() const { return tablet_.id(); }

 private:
  // zwp_tablet_v2_listener callbacks:
  static void Name(void* data, zwp_tablet_v2* tablet, const char* name);
  static void Id(void* data, zwp_tablet_v2* tablet, uint32_t vid, uint32_t pid);
  static void Path(void* data, zwp_tablet_v2* tablet, const char* path);
  static void Done(void* data, zwp_tablet_v2* tablet);
  static void Removed(void* data, zwp_tablet_v2* tablet);

  wl::Object<zwp_tablet_v2> tablet_;
  const raw_ptr<WaylandTabletSeat> seat_;
  std::string name_;
  uint32_t vid_ = 0;
  uint32_t pid_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_H_

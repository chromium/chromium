// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_MANAGER_H_

#include <cstdint>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;
class WaylandSeat;

// Wraps the zwp_tablet_manager_v2 object.
class WaylandTabletManager
    : public wl::GlobalObjectRegistrar<WaylandTabletManager> {
 public:
  static constexpr char kInterfaceName[] = "zwp_tablet_manager_v2";

  static void Instantiate(WaylandConnection* connection,
                          wl_registry* registry,
                          uint32_t name,
                          const std::string& interface,
                          uint32_t version);

  WaylandTabletManager(zwp_tablet_manager_v2* manager,
                       WaylandConnection* connection);
  WaylandTabletManager(const WaylandTabletManager&) = delete;
  WaylandTabletManager& operator=(const WaylandTabletManager&) = delete;
  ~WaylandTabletManager();

  // Creates a zwp_tablet_seat_v2 for the given seat. The ownership is passed to
  // the caller.
  wl::Object<zwp_tablet_seat_v2> GetTabletSeat(wl_seat* seat);

 private:
  wl::Object<zwp_tablet_manager_v2> manager_;
  const raw_ptr<WaylandConnection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_MANAGER_H_

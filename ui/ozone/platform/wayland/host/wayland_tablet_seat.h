// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_SEAT_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_SEAT_H_

#include <cstdint>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

struct zwp_tablet_seat_v2;

namespace ui {

class WaylandConnection;
class WaylandTablet;
class WaylandTabletTool;
class WaylandEventSource;

// Wraps the zwp_tablet_seat_v2 object.
class WaylandTabletSeat {
 public:
  WaylandTabletSeat(zwp_tablet_seat_v2* tablet_seat,
                    WaylandConnection* connection,
                    WaylandEventSource* event_source);
  WaylandTabletSeat(const WaylandTabletSeat&) = delete;
  WaylandTabletSeat& operator=(const WaylandTabletSeat&) = delete;
  ~WaylandTabletSeat();

  // Erases the tablet or tool object from the internal map.
  void OnTabletRemoved(WaylandTablet* tablet);
  void OnToolRemoved(WaylandTabletTool* tool);

 private:
  // zwp_tablet_seat_v2_listener callbacks:
  static void TabletAdded(void* data,
                          zwp_tablet_seat_v2* seat,
                          zwp_tablet_v2* tablet);
  static void ToolAdded(void* data,
                        zwp_tablet_seat_v2* seat,
                        zwp_tablet_tool_v2* tool);
  static void PadAdded(void* data,
                       zwp_tablet_seat_v2* seat,
                       zwp_tablet_pad_v2* pad);

  wl::Object<zwp_tablet_seat_v2> tablet_seat_;

  const raw_ptr<WaylandConnection> connection_;
  const raw_ptr<WaylandEventSource> event_source_;

  base::flat_map<uint32_t, std::unique_ptr<WaylandTablet>> tablets_;
  base::flat_map<uint32_t, std::unique_ptr<WaylandTabletTool>> tools_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_SEAT_H_

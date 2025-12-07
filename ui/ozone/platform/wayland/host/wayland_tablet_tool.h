// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_TOOL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_TOOL_H_

#include <cstdint>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/events/pointer_details.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"

namespace ui {

class WaylandConnection;
class WaylandTabletSeat;
class WaylandWindow;

// Wraps the zwp_tablet_tool_v2 object.
class WaylandTabletTool {
 public:
  class Delegate;

  WaylandTabletTool(zwp_tablet_tool_v2* tool,
                    WaylandTabletSeat* seat,
                    WaylandConnection* connection,
                    Delegate* delegate,
                    WaylandPointer::Delegate* pointer_delegate);
  WaylandTabletTool(const WaylandTabletTool&) = delete;
  WaylandTabletTool& operator=(const WaylandTabletTool&) = delete;
  ~WaylandTabletTool();

  uint32_t id() const { return tool_.id(); }

 private:
  // zwp_tablet_tool_v2_listener callbacks:
  static void Type(void* data, zwp_tablet_tool_v2* tool, uint32_t tool_type);
  static void HardwareSerial(void* data,
                             zwp_tablet_tool_v2* tool,
                             uint32_t serial_hi,
                             uint32_t serial_lo);
  static void HardwareIdWacom(void* data,
                              zwp_tablet_tool_v2* tool,
                              uint32_t id_hi,
                              uint32_t id_lo);
  static void Capability(void* data,
                         zwp_tablet_tool_v2* tool,
                         uint32_t capability);
  static void Done(void* data, zwp_tablet_tool_v2* tool);
  static void Removed(void* data, zwp_tablet_tool_v2* tool);
  static void ProximityIn(void* data,
                          zwp_tablet_tool_v2* tool,
                          uint32_t serial,
                          zwp_tablet_v2* tablet,
                          wl_surface* surface);
  static void ProximityOut(void* data, zwp_tablet_tool_v2* tool);
  static void Down(void* data, zwp_tablet_tool_v2* tool, uint32_t serial);
  static void Up(void* data, zwp_tablet_tool_v2* tool);
  static void Motion(void* data,
                     zwp_tablet_tool_v2* tool,
                     wl_fixed_t x,
                     wl_fixed_t y);
  static void Pressure(void* data, zwp_tablet_tool_v2* tool, uint32_t pressure);
  static void Distance(void* data, zwp_tablet_tool_v2* tool, uint32_t distance);
  static void Tilt(void* data,
                   zwp_tablet_tool_v2* tool,
                   wl_fixed_t tilt_x,
                   wl_fixed_t tilt_y);
  static void Rotation(void* data,
                       zwp_tablet_tool_v2* tool,
                       wl_fixed_t degrees);
  static void Slider(void* data, zwp_tablet_tool_v2* tool, int32_t position);
  static void Wheel(void* data,
                    zwp_tablet_tool_v2* tool,
                    wl_fixed_t degrees,
                    int32_t clicks);
  static void Button(void* data,
                     zwp_tablet_tool_v2* tool,
                     uint32_t serial,
                     uint32_t button,
                     uint32_t state);
  static void Frame(void* data, zwp_tablet_tool_v2* tool, uint32_t time);

  void DispatchBufferedEvents();
  void ResetFrameData();

  const raw_ptr<WaylandConnection> connection_;
  const raw_ptr<WaylandTabletSeat> seat_;
  const raw_ptr<Delegate> delegate_;
  const raw_ptr<WaylandPointer::Delegate> pointer_delegate_;

  wl::Object<zwp_tablet_tool_v2> tool_;

  wl::Object<wp_cursor_shape_device_v1> cursor_shape_device_;

  // Stores the state of the current event frame.
  struct FrameData {
    FrameData();
    ~FrameData();

    bool down = false;
    bool up = false;
    bool proximity_in = false;
    bool proximity_out = false;
    uint32_t proximity_serial = 0;
    base::WeakPtr<WaylandWindow> proximity_target;
    std::vector<std::pair<uint32_t, bool>> button_states;
    PointerDetails pointer_details;
    std::optional<gfx::PointF> location;
    base::TimeTicks timestamp;
  };

  FrameData frame_data_;
  EventPointerType pointer_type_ = EventPointerType::kPen;
};

class WaylandTabletTool::Delegate {
 public:
  virtual void OnTabletToolProximityIn(WaylandWindow* window,
                                       const gfx::PointF& location,
                                       const PointerDetails& details,
                                       base::TimeTicks time) = 0;
  virtual void OnTabletToolProximityOut(base::TimeTicks time) = 0;
  virtual void OnTabletToolMotion(const gfx::PointF& location,
                                  const PointerDetails& details,
                                  base::TimeTicks time) = 0;
  virtual void OnTabletToolButton(int32_t button,
                                  bool pressed,
                                  const PointerDetails& details,
                                  base::TimeTicks time) = 0;

 protected:
  virtual ~Delegate() = default;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TABLET_TOOL_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_DEVICE_EVENT_DISPATCHER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_DEVICE_EVENT_DISPATCHER_EVDEV_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "ui/events/devices/gamepad_device.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/event.h"
#include "ui/events/ozone/gamepad/gamepad_event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

struct KeyboardDevice;
struct TouchpadDevice;
enum class StylusState;

struct COMPONENT_EXPORT(EVDEV) KeyEventParams {
  KeyEventParams(int device_id,
                 int flags,
                 unsigned int code,
                 unsigned int scan_code,
                 bool down,
                 bool suppress_auto_repeat,
                 base::TimeTicks timestamp);
  KeyEventParams(const KeyEventParams& other);
  KeyEventParams() {}
  ~KeyEventParams();

  int device_id;
  int flags;
  unsigned int code;
  unsigned int scan_code;
  bool down;
  bool suppress_auto_repeat;
  base::TimeTicks timestamp;
};

struct COMPONENT_EXPORT(EVDEV) MouseMoveEventParams {
  MouseMoveEventParams(int device_id,
                       int flags,
                       const gfx::PointF& location,
                       gfx::Vector2dF* ordinal_delta,
                       const PointerDetails& details,
                       base::TimeTicks timestamp);
  MouseMoveEventParams(const MouseMoveEventParams& other);
  MouseMoveEventParams();
  ~MouseMoveEventParams();

  int device_id;
  int flags;
  gfx::PointF location;
  std::optional<gfx::Vector2dF> ordinal_delta;
  PointerDetails pointer_details;
  base::TimeTicks timestamp;
};

enum class COMPONENT_EXPORT(EVDEV) MouseButtonMapType : int {
  kNone,
  kMouse,
  kPointingStick,
  kMaxValue = kPointingStick,
};

struct COMPONENT_EXPORT(EVDEV) MouseButtonEventParams {
  MouseButtonEventParams(int device_id,
                         int flags,
                         const gfx::PointF& location,
                         unsigned int button,
                         bool down,
                         MouseButtonMapType map_type,
                         const PointerDetails& details,
                         base::TimeTicks timestamp);
  MouseButtonEventParams(const MouseButtonEventParams& other);
  MouseButtonEventParams() {}
  ~MouseButtonEventParams();

  int device_id;
  int flags;
  gfx::PointF location;
  unsigned int button;
  bool down;
  MouseButtonMapType map_type;
  PointerDetails pointer_details;
  base::TimeTicks timestamp;
};

struct COMPONENT_EXPORT(EVDEV) MouseWheelEventParams {
  MouseWheelEventParams(int device_id,
                        const gfx::PointF& location,
                        const gfx::Vector2d& delta,
                        const gfx::Vector2d& tick_120ths,
                        base::TimeTicks timestamp);
  // TODO(1077644): get rid of the MouseWheelEventParams constructor without
  // tick_120ths, once the remoting use case is updated.
  MouseWheelEventParams(int device_id,
                        const gfx::PointF& location,
                        const gfx::Vector2d& delta,
                        base::TimeTicks timestamp);
  MouseWheelEventParams(const MouseWheelEventParams& other);
  MouseWheelEventParams() {}
  ~MouseWheelEventParams();

  int device_id;
  gfx::PointF location;
  gfx::Vector2d delta;
  gfx::Vector2d tick_120ths;
  base::TimeTicks timestamp;
};

struct COMPONENT_EXPORT(EVDEV) PinchEventParams {
  PinchEventParams(int device_id,
                   EventType type,
                   const gfx::PointF location,
                   float scale,
                   const base::TimeTicks timestamp);
  PinchEventParams(const PinchEventParams& other);
  PinchEventParams() {}
  ~PinchEventParams();

  int device_id;
  EventType type;
  const gfx::PointF location;
  float scale;
  const base::TimeTicks timestamp;
};

struct COMPONENT_EXPORT(EVDEV) ScrollEventParams {
  ScrollEventParams(int device_id,
                    EventType type,
                    const gfx::PointF location,
                    const gfx::Vector2dF& delta,
                    const gfx::Vector2dF& ordinal_delta,
                    int finger_count,
                    const base::TimeTicks timestamp);
  ScrollEventParams(const ScrollEventParams& other);
  ScrollEventParams() {}
  ~ScrollEventParams();

  int device_id;
  EventType type;
  const gfx::PointF location;
  const gfx::Vector2dF delta;
  const gfx::Vector2dF ordinal_delta;
  int finger_count;
  const base::TimeTicks timestamp;
};

struct COMPONENT_EXPORT(EVDEV) TouchEventParams {
  TouchEventParams(int device_id,
                   int slot,
                   EventType type,
                   const gfx::PointF& location,
                   const PointerDetails& pointer_details,
                   const base::TimeTicks& timestamp,
                   int flags);
  TouchEventParams(const TouchEventParams& other);
  TouchEventParams() {}
  ~TouchEventParams();

  int device_id;
  int slot;
  EventType type;
  gfx::PointF location;
  PointerDetails pointer_details;
  base::TimeTicks timestamp;
  int flags;
};

// Interface used by device objects for event dispatch.
class COMPONENT_EXPORT(EVDEV) DeviceEventDispatcherEvdev {
 public:
  DeviceEventDispatcherEvdev() {}
  virtual ~DeviceEventDispatcherEvdev() {}

  // User input events.
  virtual void DispatchKeyEvent(const KeyEventParams& params) = 0;
  virtual void DispatchMouseMoveEvent(const MouseMoveEventParams& params) = 0;
  virtual void DispatchMouseButtonEvent(
      const MouseButtonEventParams& params) = 0;
  virtual void DispatchMouseWheelEvent(const MouseWheelEventParams& params) = 0;
  virtual void DispatchPinchEvent(const PinchEventParams& params) = 0;
  virtual void DispatchScrollEvent(const ScrollEventParams& params) = 0;
  virtual void DispatchTouchEvent(const TouchEventParams& params) = 0;
  virtual void DispatchGamepadEvent(const GamepadEvent& event) = 0;
  virtual void DispatchMicrophoneMuteSwitchValueChanged(bool muted) = 0;
  virtual void DispatchAnyKeysPressedUpdated(bool any) = 0;

  // Device lifecycle events.
  virtual void DispatchKeyboardDevicesUpdated(
      const std::vector<KeyboardDevice>& devices,
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) = 0;
  virtual void DispatchTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) = 0;
  virtual void DispatchMouseDevicesUpdated(
      const std::vector<InputDevice>& devices,
      bool has_mouse) = 0;
  virtual void DispatchPointingStickDevicesUpdated(
      const std::vector<InputDevice>& devices) = 0;
  virtual void DispatchTouchpadDevicesUpdated(
      const std::vector<TouchpadDevice>& devices,
      bool has_haptic_touchpad) = 0;
  virtual void DispatchGraphicsTabletDevicesUpdated(
      const std::vector<InputDevice>& devices) = 0;
  virtual void DispatchDeviceListsComplete() = 0;
  virtual void DispatchStylusStateChanged(StylusState stylus_state) = 0;
  virtual void DispatchGamepadDevicesUpdated(
      const std::vector<GamepadDevice>& devices,
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) = 0;
  virtual void DispatchUncategorizedDevicesUpdated(
      const std::vector<InputDevice>& devices) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_DEVICE_EVENT_DISPATCHER_EVDEV_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"

#include <optional>

namespace ui {

KeyEventParams::KeyEventParams(int device_id,
                               int flags,
                               unsigned int code,
                               unsigned int scan_code,
                               bool down,
                               bool suppress_auto_repeat,
                               base::TimeTicks timestamp)
    : device_id(device_id),
      flags(flags),
      code(code),
      scan_code(scan_code),
      down(down),
      suppress_auto_repeat(suppress_auto_repeat),
      timestamp(timestamp) {}

KeyEventParams::KeyEventParams(const KeyEventParams& other) = default;

KeyEventParams::~KeyEventParams() {}

MouseMoveEventParams::MouseMoveEventParams(int device_id,
                                           int flags,
                                           const gfx::PointF& location,
                                           gfx::Vector2dF* ordinal_delta,
                                           const PointerDetails& details,
                                           base::TimeTicks timestamp)
    : device_id(device_id),
      flags(flags),
      location(location),
      ordinal_delta(ordinal_delta
                        ? std::optional<gfx::Vector2dF>(*ordinal_delta)
                        : std::nullopt),
      pointer_details(details),
      timestamp(timestamp) {}

MouseMoveEventParams::MouseMoveEventParams(const MouseMoveEventParams& other) =
    default;

MouseMoveEventParams::~MouseMoveEventParams() {}

MouseButtonEventParams::MouseButtonEventParams(int device_id,
                                               int flags,
                                               const gfx::PointF& location,
                                               unsigned int button,
                                               bool down,
                                               MouseButtonMapType map_type,
                                               const PointerDetails& details,
                                               base::TimeTicks timestamp)
    : device_id(device_id),
      flags(flags),
      location(location),
      button(button),
      down(down),
      map_type(map_type),
      pointer_details(details),
      timestamp(timestamp) {}

MouseButtonEventParams::MouseButtonEventParams(
    const MouseButtonEventParams& other) = default;

MouseButtonEventParams::~MouseButtonEventParams() {
}

MouseWheelEventParams::MouseWheelEventParams(int device_id,
                                             const gfx::PointF& location,
                                             const gfx::Vector2d& delta,
                                             const gfx::Vector2d& tick_120ths,
                                             base::TimeTicks timestamp)
    : device_id(device_id),
      location(location),
      delta(delta),
      tick_120ths(tick_120ths),
      timestamp(timestamp) {}

MouseWheelEventParams::MouseWheelEventParams(int device_id,
                                             const gfx::PointF& location,
                                             const gfx::Vector2d& delta,
                                             base::TimeTicks timestamp)
    : device_id(device_id),
      location(location),
      delta(delta),
      timestamp(timestamp) {
}

MouseWheelEventParams::MouseWheelEventParams(
    const MouseWheelEventParams& other) = default;

MouseWheelEventParams::~MouseWheelEventParams() {
}

PinchEventParams::PinchEventParams(int device_id,
                                   EventType type,
                                   const gfx::PointF location,
                                   float scale,
                                   const base::TimeTicks timestamp)
    : device_id(device_id),
      type(type),
      location(location),
      scale(scale),
      timestamp(timestamp) {}

PinchEventParams::PinchEventParams(const PinchEventParams& other) = default;

PinchEventParams::~PinchEventParams() {
}

ScrollEventParams::ScrollEventParams(int device_id,
                                     EventType type,
                                     const gfx::PointF location,
                                     const gfx::Vector2dF& delta,
                                     const gfx::Vector2dF& ordinal_delta,
                                     int finger_count,
                                     const base::TimeTicks timestamp)
    : device_id(device_id),
      type(type),
      location(location),
      delta(delta),
      ordinal_delta(ordinal_delta),
      finger_count(finger_count),
      timestamp(timestamp) {
}

ScrollEventParams::ScrollEventParams(const ScrollEventParams& other) = default;

ScrollEventParams::~ScrollEventParams() {
}

TouchEventParams::TouchEventParams(int device_id,
                                   int slot,
                                   EventType type,
                                   const gfx::PointF& location,
                                   const PointerDetails& details,
                                   const base::TimeTicks& timestamp,
                                   int flags)
    : device_id(device_id),
      slot(slot),
      type(type),
      location(location),
      pointer_details(details),
      timestamp(timestamp),
      flags(flags) {}

TouchEventParams::TouchEventParams(const TouchEventParams& other) = default;

TouchEventParams::~TouchEventParams() {
}

}  // namespace ui

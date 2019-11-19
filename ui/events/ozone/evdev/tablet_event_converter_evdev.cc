// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"

#include <errno.h>
#include <linux/input.h>
#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"

namespace ui {

namespace {

// Convert tilt from [min, min + num_values) to [-90deg, +90deg)
float ScaleTilt(int value, int min_value, int num_values) {
  return 180.f * (value - min_value) / num_values - 90.f;
}

EventPointerType GetToolType(int button_tool) {
  if (button_tool == BTN_TOOL_RUBBER)
    return EventPointerType::POINTER_TYPE_ERASER;
  return EventPointerType::POINTER_TYPE_PEN;
}

}  // namespace

TabletEventConverterEvdev::TabletEventConverterEvdev(
    base::ScopedFD fd,
    base::FilePath path,
    int id,
    CursorDelegateEvdev* cursor,
    const EventDeviceInfo& info,
    DeviceEventDispatcherEvdev* dispatcher)
    : EventConverterEvdev(fd.get(),
                          path,
                          id,
                          info.device_type(),
                          info.name(),
                          info.phys(),
                          info.vendor_id(),
                          info.product_id(),
                          info.version()),
      input_device_fd_(std::move(fd)),
      controller_(FROM_HERE),
      cursor_(cursor),
      dispatcher_(dispatcher) {
  x_abs_min_ = info.GetAbsMinimum(ABS_X);
  x_abs_range_ = info.GetAbsMaximum(ABS_X) - x_abs_min_ + 1;
  y_abs_min_ = info.GetAbsMinimum(ABS_Y);
  y_abs_range_ = info.GetAbsMaximum(ABS_Y) - y_abs_min_ + 1;
  tilt_x_min_ = info.GetAbsMinimum(ABS_TILT_X);
  tilt_y_min_ = info.GetAbsMinimum(ABS_TILT_Y);
  tilt_x_range_ = info.GetAbsMaximum(ABS_TILT_X) - tilt_x_min_ + 1;
  tilt_y_range_ = info.GetAbsMaximum(ABS_TILT_Y) - tilt_y_min_ + 1;
  pressure_max_ = info.GetAbsMaximum(ABS_PRESSURE);

  if (info.HasKeyEvent(BTN_STYLUS) && !info.HasKeyEvent(BTN_STYLUS2))
    one_side_btn_pen_ = true;
}

TabletEventConverterEvdev::~TabletEventConverterEvdev() {
}

void TabletEventConverterEvdev::OnFileCanReadWithoutBlocking(int fd) {
  TRACE_EVENT1("evdev",
               "TabletEventConverterEvdev::OnFileCanReadWithoutBlocking", "fd",
               fd);

  input_event inputs[4];
  ssize_t read_size = read(fd, inputs, sizeof(inputs));
  if (read_size < 0) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }

  if (!IsEnabled())
    return;

  DCHECK_EQ(read_size % sizeof(*inputs), 0u);
  ProcessEvents(inputs, read_size / sizeof(*inputs));
}

void TabletEventConverterEvdev::ProcessEvents(const input_event* inputs,
                                              int count) {
  for (int i = 0; i < count; ++i) {
    const input_event& input = inputs[i];
    switch (input.type) {
      case EV_KEY:
        ConvertKeyEvent(input);
        break;
      case EV_ABS:
        ConvertAbsEvent(input);
        break;
      case EV_SYN:
        FlushEvents(input);
        break;
    }
  }
}

void TabletEventConverterEvdev::ConvertKeyEvent(const input_event& input) {
  // Only handle other events if we have a stylus in proximity
  if (input.code >= BTN_TOOL_PEN && input.code <= BTN_TOOL_LENS) {
    if (input.value == 1)
      stylus_ = input.code;
    else if (input.value == 0)
      stylus_ = 0;
    else
      LOG(WARNING) << "Unexpected value: " << input.value
                   << " for code: " << input.code;
  }

  if (input.code >= BTN_TOUCH && input.code <= BTN_STYLUS2) {
    DispatchMouseButton(input);
    return;
  }
}

void TabletEventConverterEvdev::ConvertAbsEvent(const input_event& input) {
  if (!cursor_)
    return;

  switch (input.code) {
    case ABS_X:
      x_abs_location_ = input.value;
      abs_value_dirty_ = true;
      break;
    case ABS_Y:
      y_abs_location_ = input.value;
      abs_value_dirty_ = true;
      break;
    case ABS_TILT_X:
      tilt_x_ = ScaleTilt(input.value, tilt_x_min_, tilt_x_range_);
      abs_value_dirty_ = true;
      break;
    case ABS_TILT_Y:
      tilt_y_ = ScaleTilt(input.value, tilt_y_min_, tilt_y_range_);
      abs_value_dirty_ = true;
      break;
    case ABS_PRESSURE:
      pressure_ = (float)input.value / pressure_max_;
      abs_value_dirty_ = true;
      break;
  }
}

void TabletEventConverterEvdev::UpdateCursor() {
  gfx::Rect confined_bounds = cursor_->GetCursorConfinedBounds();

  int x =
      ((x_abs_location_ - x_abs_min_) * confined_bounds.width()) / x_abs_range_;
  int y = ((y_abs_location_ - y_abs_min_) * confined_bounds.height()) /
          y_abs_range_;

  x += confined_bounds.x();
  y += confined_bounds.y();

  cursor_->MoveCursorTo(gfx::PointF(x, y));
}

void TabletEventConverterEvdev::DispatchMouseButton(const input_event& input) {
  if (!cursor_)
    return;

  unsigned int button;
  // These are the same as X11 behaviour
  if (input.code == BTN_TOUCH) {
    button = BTN_LEFT;
  } else if (input.code == BTN_STYLUS2) {
    button = BTN_RIGHT;
  } else if (input.code == BTN_STYLUS) {
    if (one_side_btn_pen_)
      button = BTN_RIGHT;
    else
      button = BTN_MIDDLE;
  } else {
    return;
  }

  if (abs_value_dirty_) {
    UpdateCursor();
    abs_value_dirty_ = false;
  }

  bool down = input.value;

  dispatcher_->DispatchMouseButtonEvent(MouseButtonEventParams(
      input_device_.id, EF_NONE, cursor_->GetLocation(), button, down,
      false /* allow_remap */,
      PointerDetails(GetToolType(stylus_), /* pointer_id*/ 0,
                     /* radius_x */ 0.0f, /* radius_y */ 0.0f, pressure_,
                     /* twist */ 0.0f, tilt_x_, tilt_y_),
      TimeTicksFromInputEvent(input)));
}

void TabletEventConverterEvdev::FlushEvents(const input_event& input) {
  if (!cursor_)
    return;

  // Prevent propagation of invalid data on stylus lift off
  if (stylus_ == 0) {
    abs_value_dirty_ = false;
    return;
  }

  if (!abs_value_dirty_)
    return;

  UpdateCursor();

  dispatcher_->DispatchMouseMoveEvent(MouseMoveEventParams(
      input_device_.id, EF_NONE, cursor_->GetLocation(),
      PointerDetails(GetToolType(stylus_), /* pointer_id*/ 0,
                     /* radius_x */ 0.0f, /* radius_y */ 0.0f, pressure_,
                     /* twist */ 0.0f, tilt_x_, tilt_y_),
      TimeTicksFromInputEvent(input)));

  abs_value_dirty_ = false;
}

}  // namespace ui

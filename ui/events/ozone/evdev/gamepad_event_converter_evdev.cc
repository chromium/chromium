// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/gamepad_event_converter_evdev.h"

#include <errno.h>
#include <linux/input.h>
#include <stddef.h>

#include "base/trace_event/trace_event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/gamepad/gamepad_event.h"
#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"

namespace ui {

GamepadEventConverterEvdev::GamepadEventConverterEvdev(
    base::ScopedFD fd,
    base::FilePath path,
    int id,
    const EventDeviceInfo& devinfo,
    DeviceEventDispatcherEvdev* dispatcher)
    : EventConverterEvdev(fd.get(),
                          path,
                          id,
                          devinfo.device_type(),
                          devinfo.name(),
                          devinfo.phys(),
                          devinfo.vendor_id(),
                          devinfo.product_id(),
                          devinfo.version()),
      will_send_frame_(false),
      input_device_fd_(std::move(fd)),
      dispatcher_(dispatcher) {
  input_absinfo abs_info;
  for (int code = 0; code < ABS_CNT; ++code) {
    abs_info = devinfo.GetAbsInfoByCode(code);
    if (devinfo.HasAbsEvent(code)) {
      ui::GamepadDevice::Axis axis;
      axis.code = code;
      axis.min_value = abs_info.minimum;
      axis.max_value = abs_info.maximum;
      axis.flat = abs_info.flat;
      axis.fuzz = abs_info.fuzz;
      axis.resolution = abs_info.resolution;
      axes_.push_back(axis);
    }
  }
}

GamepadEventConverterEvdev::~GamepadEventConverterEvdev() {
  DCHECK(!IsEnabled());
}

bool GamepadEventConverterEvdev::HasGamepad() const {
  return true;
}

void GamepadEventConverterEvdev::OnFileCanReadWithoutBlocking(int fd) {
  TRACE_EVENT1("evdev",
               "GamepadEventConverterEvdev::OnFileCanReadWithoutBlocking", "fd",
               fd);
  while (true) {
    input_event input;
    ssize_t read_size = read(fd, &input, sizeof(input));
    if (read_size != sizeof(input)) {
      if (errno == EINTR || errno == EAGAIN)
        return;
      if (errno != ENODEV)
        PLOG(ERROR) << "error reading device " << path_.value();
      Stop();
      return;
    }

    if (!IsEnabled())
      return;

    ProcessEvent(input);
  }
}
void GamepadEventConverterEvdev::OnDisabled() {
  ResetGamepad();
}

std::vector<ui::GamepadDevice::Axis>
GamepadEventConverterEvdev::GetGamepadAxes() const {
  return axes_;
}

void GamepadEventConverterEvdev::ProcessEvent(const input_event& evdev_ev) {
  base::TimeTicks timestamp = TimeTicksFromInputEvent(evdev_ev);
  // We may have missed Gamepad releases. Reset everything.
  // If the event is sync, we send a frame.
  if (evdev_ev.type == EV_SYN) {
    if (evdev_ev.code == SYN_DROPPED) {
      LOG(WARNING) << "kernel dropped input events";
      ResyncGamepad();
    } else if (evdev_ev.code == SYN_REPORT) {
      OnSync(timestamp);
    }
  } else if (evdev_ev.type == EV_KEY) {
    ProcessEvdevKey(evdev_ev.code, evdev_ev.value, timestamp);
  } else if (evdev_ev.type == EV_ABS) {
    ProcessEvdevAbs(evdev_ev.code, evdev_ev.value, timestamp);
  }
}

void GamepadEventConverterEvdev::ProcessEvdevKey(
    uint16_t code,
    int value,
    const base::TimeTicks& timestamp) {
  if (value)
    pressed_buttons_.insert(code);
  else
    pressed_buttons_.erase(code);
  GamepadEvent event(input_device_.id, GamepadEventType::BUTTON, code, value,
                     timestamp);
  dispatcher_->DispatchGamepadEvent(event);
  will_send_frame_ = true;
}

void GamepadEventConverterEvdev::ProcessEvdevAbs(
    uint16_t code,
    int value,
    const base::TimeTicks& timestamp) {
  GamepadEvent event(input_device_.id, GamepadEventType::AXIS, code, value,
                     timestamp);
  dispatcher_->DispatchGamepadEvent(event);
  will_send_frame_ = true;
}

void GamepadEventConverterEvdev::ResetGamepad() {
  base::TimeTicks timestamp = ui::EventTimeForNow();
  // Reset all the buttons.
  for (unsigned int code : pressed_buttons_)
    ProcessEvdevKey(code, 0, timestamp);
  // Reset all the axes.
  for (int code = 0; code < ABS_CNT; ++code)
    ProcessEvdevAbs(code, 0, timestamp);
  OnSync(timestamp);
}

void GamepadEventConverterEvdev::ResyncGamepad() {
  base::TimeTicks timestamp = ui::EventTimeForNow();
  // Reset all the buttons.
  for (unsigned int code : pressed_buttons_)
    ProcessEvdevKey(code, 0, timestamp);
  // Read the state of all axis.
  EventDeviceInfo info;
  if (!info.Initialize(fd_, path_)) {
    LOG(ERROR) << "Failed to synchronize state for gamepad device: "
               << path_.value();
    Stop();
    return;
  }
  for (int code = 0; code < ABS_CNT; ++code) {
    if (info.HasAbsEvent(code)) {
      ProcessEvdevAbs(code, info.GetAbsValue(code), timestamp);
    }
  }
  OnSync(timestamp);
}

void GamepadEventConverterEvdev::OnSync(const base::TimeTicks& timestamp) {
  if (will_send_frame_) {
    GamepadEvent event(input_device_.id, GamepadEventType::FRAME, 0, 0,
                       timestamp);
    dispatcher_->DispatchGamepadEvent(event);
    will_send_frame_ = false;
  }
}
}  //  namespace ui

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <linux/input.h>
#include <stddef.h>

#include "ui/events/ozone/evdev/event_converter_evdev.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_utils.h"

namespace ui {

EventConverterEvdev::EventConverterEvdev(int fd,
                                         const base::FilePath& path,
                                         int id,
                                         InputDeviceType type,
                                         const std::string& name,
                                         const std::string& phys,
                                         uint16_t vendor_id,
                                         uint16_t product_id,
                                         uint16_t version)
    : fd_(fd),
      path_(path),
      input_device_(id,
                    type,
                    name,
                    phys,
                    GetInputPathInSys(path),
                    vendor_id,
                    product_id,
                    version),
      controller_(FROM_HERE) {
  input_device_.enabled = false;
}

EventConverterEvdev::~EventConverterEvdev() {
}

void EventConverterEvdev::Start() {
  base::MessageLoopCurrentForUI::Get()->WatchFileDescriptor(
      fd_, true, base::MessagePumpForUI::WATCH_READ, &controller_, this);
  watching_ = true;
}

void EventConverterEvdev::Stop() {
  controller_.StopWatchingFileDescriptor();
  watching_ = false;
}

void EventConverterEvdev::SetEnabled(bool enabled) {
  if (enabled == input_device_.enabled)
    return;
  if (enabled) {
    TRACE_EVENT1("evdev", "EventConverterEvdev::OnEnabled", "path",
                 path_.value());
    OnEnabled();
  } else {
    TRACE_EVENT1("evdev", "EventConverterEvdev::OnDisabled", "path",
                 path_.value());
    OnDisabled();
  }
  input_device_.enabled = enabled;
}

bool EventConverterEvdev::IsEnabled() const {
  return input_device_.enabled;
}

void EventConverterEvdev::OnStopped() {
}

void EventConverterEvdev::OnEnabled() {
}

void EventConverterEvdev::OnDisabled() {
}

void EventConverterEvdev::DumpTouchEventLog(const char* filename) {
}

void EventConverterEvdev::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

bool EventConverterEvdev::HasKeyboard() const {
  return false;
}

bool EventConverterEvdev::HasMouse() const {
  return false;
}

bool EventConverterEvdev::HasTouchpad() const {
  return false;
}

bool EventConverterEvdev::HasTouchscreen() const {
  return false;
}

bool EventConverterEvdev::HasPen() const {
  return false;
}

bool EventConverterEvdev::HasGamepad() const {
  return false;
}

bool EventConverterEvdev::HasCapsLockLed() const {
  return false;
}

gfx::Size EventConverterEvdev::GetTouchscreenSize() const {
  NOTREACHED();
  return gfx::Size();
}

std::vector<ui::GamepadDevice::Axis> EventConverterEvdev::GetGamepadAxes()
    const {
  NOTREACHED();
  return std::vector<ui::GamepadDevice::Axis>();
}

int EventConverterEvdev::GetTouchPoints() const {
  NOTREACHED();
  return 0;
}

void EventConverterEvdev::SetKeyFilter(bool enable_filter,
                                       std::vector<DomCode> allowed_keys) {
  NOTREACHED();
}

void EventConverterEvdev::SetCapsLockLed(bool enabled) {
  if (!HasCapsLockLed())
    return;

  input_event events[2];
  memset(&events, 0, sizeof(events));

  events[0].type = EV_LED;
  events[0].code = LED_CAPSL;
  events[0].value = enabled;

  events[1].type = EV_SYN;
  events[1].code = SYN_REPORT;
  events[1].value = 0;

  ssize_t written = write(fd_, events, sizeof(events));

  if (written < 0) {
    if (errno != ENODEV)
      PLOG(ERROR) << "cannot set leds for " << path_.value() << ":";
    Stop();
  } else if (written != sizeof(events)) {
    LOG(ERROR) << "short write setting leds for " << path_.value();
    Stop();
  }
}

void EventConverterEvdev::SetTouchEventLoggingEnabled(bool enabled) {
}

void EventConverterEvdev::SetPalmSuppressionCallback(
    const base::RepeatingCallback<void(bool)>& callback) {}

base::TimeTicks EventConverterEvdev::TimeTicksFromInputEvent(
    const input_event& event) {
  base::TimeTicks timestamp =
      ui::EventTimeStampFromSeconds(event.time.tv_sec) +
      base::TimeDelta::FromMicroseconds(event.time.tv_usec);
  ValidateEventTimeClock(&timestamp);
  return timestamp;
}
}  // namespace ui

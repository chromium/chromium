// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"

#include <errno.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace ui {

namespace {

std::string FormatLog(const char* fmt, va_list args) {
  std::string msg = base::StringPrintV(fmt, args);
  if (!msg.empty() && msg.back() == '\n')
    msg.pop_back();
  return msg;
}

}  // namespace

EventReaderLibevdevCros::EventReaderLibevdevCros(
    base::ScopedFD fd,
    const base::FilePath& path,
    int id,
    const EventDeviceInfo& devinfo,
    std::unique_ptr<Delegate> delegate)
    : EventConverterEvdev(fd.get(),
                          path,
                          id,
                          devinfo.device_type(),
                          devinfo.name(),
                          devinfo.phys(),
                          devinfo.vendor_id(),
                          devinfo.product_id(),
                          devinfo.version()),
      has_keyboard_(devinfo.HasKeyboard()),
      has_mouse_(devinfo.HasMouse()),
      has_touchpad_(devinfo.HasTouchpad()),
      has_caps_lock_led_(devinfo.HasLedEvent(LED_CAPSL)),
      delegate_(std::move(delegate)) {
  // This class assumes it does not deal with internal keyboards.
  CHECK(!has_keyboard_ || type() != INPUT_DEVICE_INTERNAL);

  memset(&evdev_, 0, sizeof(evdev_));
  evdev_.log = OnLogMessage;
  evdev_.log_udata = this;
  evdev_.syn_report = OnSynReport;
  evdev_.syn_report_udata = this;
  evdev_.fd = fd.release();

  memset(&evstate_, 0, sizeof(evstate_));
  evdev_.evstate = &evstate_;
  Event_Init(&evdev_);

  Event_Open(&evdev_);

  delegate_->OnLibEvdevCrosOpen(&evdev_, &evstate_);
}

EventReaderLibevdevCros::~EventReaderLibevdevCros() {
  Stop();
  EvdevClose(&evdev_);
}

EventReaderLibevdevCros::Delegate::~Delegate() {}

void EventReaderLibevdevCros::OnFileCanReadWithoutBlocking(int fd) {
  TRACE_EVENT1("evdev", "EventReaderLibevdevCros::OnFileCanReadWithoutBlocking",
               "fd", fd);

  if (EvdevRead(&evdev_)) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }
}

bool EventReaderLibevdevCros::HasKeyboard() const {
  return has_keyboard_;
}

bool EventReaderLibevdevCros::HasMouse() const {
  return has_mouse_;
}

bool EventReaderLibevdevCros::HasTouchpad() const {
  return has_touchpad_;
}

bool EventReaderLibevdevCros::HasCapsLockLed() const {
  return has_caps_lock_led_;
}

void EventReaderLibevdevCros::OnDisabled() {
  delegate_->OnLibEvdevCrosStopped(&evdev_, &evstate_);
}

// static
void EventReaderLibevdevCros::OnSynReport(void* data,
                                          EventStateRec* evstate,
                                          struct timeval* tv) {
  EventReaderLibevdevCros* reader = static_cast<EventReaderLibevdevCros*>(data);
  if (!reader->IsEnabled())
    return;

  reader->delegate_->OnLibEvdevCrosEvent(&reader->evdev_, evstate, *tv);
}

// static
void EventReaderLibevdevCros::OnLogMessage(void* data,
                                           int level,
                                           const char* fmt,
                                           ...) {
  va_list args;
  va_start(args, fmt);
  if (level >= LOGLEVEL_ERROR)
    LOG(ERROR) << "libevdev: " << FormatLog(fmt, args);
  else if (level >= LOGLEVEL_WARNING)
    LOG(WARNING) << "libevdev: " << FormatLog(fmt, args);
  else
    DVLOG(3) << "libevdev: " << FormatLog(fmt, args);
  va_end(args);
}

}  // namespace ui

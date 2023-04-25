// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/stylus_button_event_converter_evdev.h"

#include <errno.h>
#include <linux/input.h>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"

namespace ui {

namespace {

const int kKeyReleaseValue = 0;

}  // namespace

StylusButtonEventConverterEvdev::StylusButtonEventConverterEvdev(
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
      input_device_fd_(std::move(fd)),
      dispatcher_(dispatcher) {}

StylusButtonEventConverterEvdev::~StylusButtonEventConverterEvdev() {}

void StylusButtonEventConverterEvdev::OnFileCanReadWithoutBlocking(int fd) {
  TRACE_EVENT1("evdev",
               "StylusButtonEventConverterEvdev::OnFileCanReadWithoutBlocking",
               "fd", fd);

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

    ProcessEvent(input);
  }
}

void StylusButtonEventConverterEvdev::ProcessEvent(const input_event& input) {
  if (input.type == EV_KEY && input.code == KEY_F19) {
    bool down = input.value != kKeyReleaseValue;
    dispatcher_->DispatchKeyEvent(
        KeyEventParams(input_device_.id, ui::EF_IS_STYLUS_BUTTON, input.code,
                       0 /* scan_code */, down, true /* suppress_auto_repeat */,
                       TimeTicksFromInputEvent(input)));
  }
}

std::ostream& StylusButtonEventConverterEvdev::DescribeForLog(
    std::ostream& os) const {
  os << "class=ui::StylusButtonEventConverterEvdev id=" << input_device_.id
     << std::endl
     << "base ";
  return EventConverterEvdev::DescribeForLog(os);
}

}  // namespace ui

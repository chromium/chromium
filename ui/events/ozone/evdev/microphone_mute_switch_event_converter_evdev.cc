// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/microphone_mute_switch_event_converter_evdev.h"

#include <errno.h>
#include <linux/input.h>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_device_util.h"

namespace ui {

namespace {

int32_t GetSwValue(int fd, unsigned int code) {
  unsigned long bitmask[EVDEV_BITS_TO_LONGS(SW_MAX)] = {0};
  if (ioctl(fd, EVIOCGSW(sizeof(bitmask)), bitmask) < 0) {
    PLOG(ERROR) << "Failed EVIOCGSW";
    return 0;
  }
  return EvdevBitIsSet(bitmask, code);
}

}  // namespace

MicrophoneMuteSwitchEventConverterEvdev::
    MicrophoneMuteSwitchEventConverterEvdev(
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
      dispatcher_(dispatcher) {
  DCHECK(devinfo.IsMicrophoneMuteSwitchDevice());
}

MicrophoneMuteSwitchEventConverterEvdev::
    ~MicrophoneMuteSwitchEventConverterEvdev() = default;

void MicrophoneMuteSwitchEventConverterEvdev::OnDisabled() {
  dispatcher_->DispatchMicrophoneMuteSwitchValueChanged(false);
}

void MicrophoneMuteSwitchEventConverterEvdev::OnEnabled() {
  // Send out the initial switch state, as clients (e.g. system UI) depend on
  // the current mute switch state.
  dispatcher_->DispatchMicrophoneMuteSwitchValueChanged(
      GetSwValue(input_device_fd_.get(), SW_MUTE_DEVICE));
}

void MicrophoneMuteSwitchEventConverterEvdev::OnFileCanReadWithoutBlocking(
    int fd) {
  TRACE_EVENT1(
      "evdev",
      "MicrophoneMuteSwitchEventConverterEvdev::OnFileCanReadWithoutBlocking",
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

void MicrophoneMuteSwitchEventConverterEvdev::ProcessEvent(
    const input_event& input) {
  if (input.type == EV_SW && input.code == SW_MUTE_DEVICE)
    dispatcher_->DispatchMicrophoneMuteSwitchValueChanged(input.value);
}

std::ostream& MicrophoneMuteSwitchEventConverterEvdev::DescribeForLog(
    std::ostream& os) const {
  os << "class=ui::MicrophoneMuteSwitchEventConverterEvdev id="
     << input_device_.id << std::endl
     << "base ";
  return EventConverterEvdev::DescribeForLog(os);
}

}  // namespace ui

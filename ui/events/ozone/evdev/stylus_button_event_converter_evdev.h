// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_STYLUS_BUTTON_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_STYLUS_BUTTON_EVENT_CONVERTER_EVDEV_H_

#include <ostream>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

class DeviceEventDispatcherEvdev;

class COMPONENT_EXPORT(EVDEV) StylusButtonEventConverterEvdev
    : public EventConverterEvdev {
 public:
  StylusButtonEventConverterEvdev(base::ScopedFD fd,
                                  base::FilePath path,
                                  int id,
                                  const EventDeviceInfo& devinfo,
                                  DeviceEventDispatcherEvdev* dispatcher);

  StylusButtonEventConverterEvdev(const StylusButtonEventConverterEvdev&) =
      delete;
  StylusButtonEventConverterEvdev& operator=(
      const StylusButtonEventConverterEvdev&) = delete;

  ~StylusButtonEventConverterEvdev() override;

  // EventConverterEvdev
  void OnFileCanReadWithoutBlocking(int fd) override;

  void ProcessEvent(const struct input_event& input);

  std::ostream& DescribeForLog(std::ostream& os) const override;

 private:
  friend class MockStylusButtonEventConverterEvdev;

  // Input device file descriptor.
  const base::ScopedFD input_device_fd_;

  // Callbacks for dispatching events.
  const raw_ptr<DeviceEventDispatcherEvdev> dispatcher_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_STYLUS_BUTTON_EVENT_CONVERTER_EVDEV_H_

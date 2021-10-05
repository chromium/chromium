// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libinput_event_converter.h"

#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

LibInputEventConverter::LibInputEventConverter(int fd,
                                               const base::FilePath& path,
                                               int id,
                                               const EventDeviceInfo& devinfo)
    : EventConverterEvdev(fd,
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
      has_touchscreen_(devinfo.HasTouchscreen()) {}

LibInputEventConverter::~LibInputEventConverter() {}

bool LibInputEventConverter::HasKeyboard() const {
  return has_keyboard_;
}

bool LibInputEventConverter::HasMouse() const {
  return has_mouse_;
}

bool LibInputEventConverter::HasTouchpad() const {
  return has_touchpad_;
}

bool LibInputEventConverter::HasTouchscreen() const {
  return has_touchscreen_;
}

}  // namespace ui

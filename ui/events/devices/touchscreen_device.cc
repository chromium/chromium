// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/touchscreen_device.h"

#include <ostream>
#include <string>

#include "ui/events/devices/input_device.h"

namespace ui {

TouchscreenDevice::TouchscreenDevice() = default;

TouchscreenDevice::TouchscreenDevice(int id,
                                     InputDeviceType type,
                                     const std::string& name,
                                     const gfx::Size& size,
                                     int touch_points,
                                     bool has_stylus,
                                     bool has_stylus_garage_switch)
    : InputDevice(id, type, name),
      size(size),
      touch_points(touch_points),
      has_stylus(has_stylus),
      has_stylus_garage_switch(has_stylus_garage_switch) {}

TouchscreenDevice::TouchscreenDevice(const InputDevice& input_device,
                                     const gfx::Size& size,
                                     int touch_points,
                                     bool has_stylus,
                                     bool has_stylus_garage_switch)
    : InputDevice(input_device),
      size(size),
      touch_points(touch_points),
      has_stylus(has_stylus),
      has_stylus_garage_switch(has_stylus_garage_switch) {}

TouchscreenDevice::TouchscreenDevice(const TouchscreenDevice& other) = default;

TouchscreenDevice::~TouchscreenDevice() = default;

std::ostream& TouchscreenDevice::DescribeForLog(std::ostream& os) const {
  os << "class=ui::TouchscreenDevice id=" << id << std::endl
     << " size=" << size.ToString() << std::endl
     << " touch_points=" << touch_points << std::endl
     << " has_stylus=" << has_stylus << std::endl
     << " has_stylus_garage_switch=" << has_stylus_garage_switch << std::endl
     << "base ";
  return InputDevice::DescribeForLog(os);
}

}  // namespace ui

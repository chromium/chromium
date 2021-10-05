// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBINPUT_EVENT_CONVERTER_H_
#define UI_EVENTS_OZONE_EVDEV_LIBINPUT_EVENT_CONVERTER_H_

#include "ui/events/ozone/evdev/event_converter_evdev.h"

namespace ui {

class EventDeviceInfo;

// Use libinput (freedesktop.org/wiki/Software/libinput) to read events from
// old touchpads that are not supported by libgestures.
//
// Note that although libinput can read inputs from any number of devices, this
// implementation only attaches one device per libinput context.
class LibInputEventConverter : public EventConverterEvdev {
 public:
  LibInputEventConverter(int fd,
                         const base::FilePath& path,
                         int id,
                         const EventDeviceInfo& devinfo);
  LibInputEventConverter(const LibInputEventConverter& other) = delete;
  LibInputEventConverter& operator=(const LibInputEventConverter& other) =
      delete;
  ~LibInputEventConverter() override;

  bool HasKeyboard() const final;

  bool HasMouse() const final;

  bool HasTouchpad() const final;

  bool HasTouchscreen() const final;

 private:
  const bool has_keyboard_;
  const bool has_mouse_;
  const bool has_touchpad_;
  const bool has_touchscreen_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_LIBINPUT_EVENT_CONVERTER_H_

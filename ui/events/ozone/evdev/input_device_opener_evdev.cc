// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_opener_evdev.h"

#include <fcntl.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/event_switches.h"
#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"
#include "ui/events/ozone/evdev/gamepad_event_converter_evdev.h"
#include "ui/events/ozone/evdev/microphone_mute_switch_event_converter_evdev.h"
#include "ui/events/ozone/evdev/stylus_button_event_converter_evdev.h"
#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"
#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"

#if defined(USE_EVDEV_GESTURES)
#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"
#endif

#if defined(USE_LIBINPUT)
#include "ui/events/ozone/evdev/libinput_event_converter.h"
#endif

#ifndef EVIOCSCLOCKID
#define EVIOCSCLOCKID _IOW('E', 0xa0, int)
#endif

namespace ui {

namespace {

std::unique_ptr<EventConverterEvdev> CreateConverter(
    const OpenInputDeviceParams& params,
    base::ScopedFD fd,
    const EventDeviceInfo& devinfo) {
#if defined(USE_LIBINPUT)
  // Use LibInputEventConverter for odd touchpads
  if (devinfo.UseLibinput()) {
    return LibInputEventConverter::Create(params.path, params.id, devinfo,
                                          params.cursor, params.dispatcher);
  }
#endif

#if defined(USE_EVDEV_GESTURES)
  // Touchpad or mouse: use gestures library.
  // EventReaderLibevdevCros -> GestureInterpreterLibevdevCros -> DispatchEvent
  if (devinfo.HasTouchpad() || devinfo.HasMouse() ||
      devinfo.HasPointingStick()) {
    std::unique_ptr<GestureInterpreterLibevdevCros> gesture_interp =
        std::make_unique<GestureInterpreterLibevdevCros>(
            params.id, params.cursor, params.gesture_property_provider,
            params.dispatcher);
    return std::make_unique<EventReaderLibevdevCros>(std::move(fd), params.path,
                                                     params.id, devinfo,
                                                     std::move(gesture_interp));
  }
#endif

  // Touchscreen: use TouchEventConverterEvdev.
  if (devinfo.HasTouchscreen()) {
    return TouchEventConverterEvdev::Create(
        std::move(fd), params.path, params.id, devinfo,
        params.shared_palm_state, params.dispatcher);
  }

  // Graphics tablet
  if (devinfo.HasTablet()) {
    return base::WrapUnique<EventConverterEvdev>(new TabletEventConverterEvdev(
        std::move(fd), params.path, params.id, params.cursor, devinfo,
        params.dispatcher));
  }

  if (devinfo.HasGamepad()) {
    return base::WrapUnique<EventConverterEvdev>(new GamepadEventConverterEvdev(
        std::move(fd), params.path, params.id, devinfo, params.dispatcher));
  }

  if (devinfo.IsStylusButtonDevice()) {
    return base::WrapUnique<EventConverterEvdev>(
        new StylusButtonEventConverterEvdev(
            std::move(fd), params.path, params.id, devinfo, params.dispatcher));
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMicrophoneMuteSwitchDeviceSwitch) &&
      devinfo.IsMicrophoneMuteSwitchDevice()) {
    return base::WrapUnique<EventConverterEvdev>(
        new MicrophoneMuteSwitchEventConverterEvdev(
            std::move(fd), params.path, params.id, devinfo, params.dispatcher));
  }

  // Everything else: use EventConverterEvdevImpl.
  return base::WrapUnique<EventConverterEvdevImpl>(
      new EventConverterEvdevImpl(std::move(fd), params.path, params.id,
                                  devinfo, params.cursor, params.dispatcher));
}

}  // namespace

std::unique_ptr<EventConverterEvdev> InputDeviceOpenerEvdev::OpenInputDevice(
    const OpenInputDeviceParams& params) {
  const base::FilePath& path = params.path;
  TRACE_EVENT1("evdev", "OpenInputDevice", "path", path.value());

  base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_NONBLOCK));
  if (fd.get() < 0) {
    PLOG(ERROR) << "Cannot open " << path.value();
    return nullptr;
  }

  // Use monotonic timestamps for events. The touch code in particular
  // expects event timestamps to correlate to the monotonic clock
  // (base::TimeTicks).
  unsigned int clk = CLOCK_MONOTONIC;
  if (ioctl(fd.get(), EVIOCSCLOCKID, &clk))
    PLOG(ERROR) << "failed to set CLOCK_MONOTONIC";

  EventDeviceInfo devinfo;
  if (!devinfo.Initialize(fd.get(), path)) {
    LOG(ERROR) << "Failed to get device information for " << path.value();
    return nullptr;
  }

  return CreateConverter(params, std::move(fd), devinfo);
}

}  // namespace ui

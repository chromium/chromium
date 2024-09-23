// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/test/events_test_utils_x11.h"

#include <stddef.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/devices/x11/xinput_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xinput.h"
#include "ui/gfx/x/xproto.h"

namespace {

// Converts ui::EventType to state for X*Events.
x11::KeyButMask XEventState(int flags) {
  constexpr auto kNoMask = x11::KeyButMask{};
  return ((flags & ui::EF_SHIFT_DOWN) ? x11::KeyButMask::Shift : kNoMask) |
         ((flags & ui::EF_CAPS_LOCK_ON) ? x11::KeyButMask::Lock : kNoMask) |
         ((flags & ui::EF_CONTROL_DOWN) ? x11::KeyButMask::Control : kNoMask) |
         ((flags & ui::EF_ALT_DOWN) ? x11::KeyButMask::Mod1 : kNoMask) |
         ((flags & ui::EF_NUM_LOCK_ON) ? x11::KeyButMask::Mod2 : kNoMask) |
         ((flags & ui::EF_MOD3_DOWN) ? x11::KeyButMask::Mod3 : kNoMask) |
         ((flags & ui::EF_COMMAND_DOWN) ? x11::KeyButMask::Mod4 : kNoMask) |
         ((flags & ui::EF_ALTGR_DOWN) ? x11::KeyButMask::Mod5 : kNoMask) |
         ((flags & ui::EF_LEFT_MOUSE_BUTTON) ? x11::KeyButMask::Button1
                                             : kNoMask) |
         ((flags & ui::EF_MIDDLE_MOUSE_BUTTON) ? x11::KeyButMask::Button2
                                               : kNoMask) |
         ((flags & ui::EF_RIGHT_MOUSE_BUTTON) ? x11::KeyButMask::Button3
                                              : kNoMask);
}

// Converts EventType to XKeyEvent type.
x11::KeyEvent::Opcode XKeyEventType(ui::EventType type) {
  switch (type) {
    case ui::EventType::kKeyPressed:
      return x11::KeyEvent::Press;
    case ui::EventType::kKeyReleased:
      return x11::KeyEvent::Release;
    default:
      NOTREACHED_IN_MIGRATION();
      return {};
  }
}

// Converts EventType to XI2 event type.
int XIKeyEventType(ui::EventType type) {
  switch (type) {
    case ui::EventType::kKeyPressed:
      return x11::Input::DeviceEvent::KeyPress;
    case ui::EventType::kKeyReleased:
      return x11::Input::DeviceEvent::KeyRelease;
    default:
      return 0;
  }
}

int XIButtonEventType(ui::EventType type) {
  switch (type) {
    case ui::EventType::kMousewheel:
    case ui::EventType::kMousePressed:
      // The button release X events for mouse wheels are dropped by Aura.
      return x11::Input::DeviceEvent::ButtonPress;
    case ui::EventType::kMouseReleased:
      return x11::Input::DeviceEvent::ButtonRelease;
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

// Converts Aura event type and flag to X button event.
unsigned int XButtonEventButton(ui::EventType type, int flags) {
  // Aura events don't keep track of mouse wheel button, so just return
  // the first mouse wheel button.
  if (type == ui::EventType::kMousewheel) {
    return 4;
  }

  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    return 1;
  if (flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    return 2;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    return 3;

  return 0;
}

void InitValuatorsForXIDeviceEvent(x11::Input::DeviceEvent* devev) {
  int valuator_count = ui::DeviceDataManagerX11::DT_LAST_ENTRY;
  auto mask_len = (valuator_count / 8) + 1;
  devev->valuator_mask.resize((mask_len + 3) / 4);
  devev->axisvalues.resize(valuator_count);
}

template <typename T>
x11::Input::Fp1616 ToFp1616(T x) {
  return static_cast<x11::Input::Fp1616>(x * (1 << 16));
}

x11::Event CreateXInput2Event(int deviceid,
                              int evtype,
                              int tracking_id,
                              const gfx::Point& location) {
  x11::Input::DeviceEvent event;
  event.deviceid = static_cast<x11::Input::DeviceId>(deviceid);
  event.sourceid = static_cast<x11::Input::DeviceId>(deviceid);
  event.opcode = static_cast<x11::Input::DeviceEvent::Opcode>(evtype);
  event.detail = tracking_id;
  event.event_x = ToFp1616(location.x()),
  event.event_y = ToFp1616(location.y()),
  event.event = x11::Connection::Get()->default_root();
  event.button_mask = {0, 0};
  return x11::Event(false, std::move(event));
}

}  // namespace

namespace ui {

ScopedXI2Event::ScopedXI2Event() = default;
ScopedXI2Event::~ScopedXI2Event() = default;

void ScopedXI2Event::InitKeyEvent(EventType type,
                                  KeyboardCode key_code,
                                  int flags) {
  x11::KeyEvent key_event{
      .opcode = XKeyEventType(type),
      .detail = static_cast<x11::KeyCode>(
          XKeyCodeForWindowsKeyCode(key_code, flags, x11::Connection::Get())),
      .state = static_cast<x11::KeyButMask>(XEventState(flags)),
      .same_screen = true,
  };

  x11::Event x11_event(false, key_event);
  event_ = std::move(x11_event);
}

void ScopedXI2Event::InitMotionEvent(const gfx::Point& location,
                                     const gfx::Point& root_location,
                                     int flags) {
  x11::MotionNotifyEvent motion_event{
      .root_x = static_cast<int16_t>(root_location.x()),
      .root_y = static_cast<int16_t>(root_location.y()),
      .event_x = static_cast<int16_t>(location.x()),
      .event_y = static_cast<int16_t>(location.y()),
      .state = static_cast<x11::KeyButMask>(XEventState(flags)),
      .same_screen = true,
  };

  x11::Event x11_event(false, motion_event);
  event_ = std::move(x11_event);
}

void ScopedXI2Event::InitButtonEvent(EventType type,
                                     const gfx::Point& location,
                                     int flags) {
  x11::ButtonEvent button_event{
      .opcode = type == ui::EventType::kMousePressed
                    ? x11::ButtonEvent::Press
                    : x11::ButtonEvent::Release,
      .detail = static_cast<x11::Button>(XButtonEventButton(type, flags)),
      .root_x = static_cast<int16_t>(location.x()),
      .root_y = static_cast<int16_t>(location.y()),
      .event_x = static_cast<int16_t>(location.x()),
      .event_y = static_cast<int16_t>(location.y()),
      .same_screen = true,
  };

  x11::Event x11_event(false, button_event);
  event_ = std::move(x11_event);
}

void ScopedXI2Event::InitGenericKeyEvent(int deviceid,
                                         int sourceid,
                                         EventType type,
                                         KeyboardCode key_code,
                                         int flags) {
  event_ = CreateXInput2Event(deviceid, XIKeyEventType(type), 0, gfx::Point());
  auto* dev_event = event_.As<x11::Input::DeviceEvent>();
  dev_event->mods.effective = static_cast<uint32_t>(XEventState(flags));
  dev_event->detail =
      XKeyCodeForWindowsKeyCode(key_code, flags, x11::Connection::Get());
  dev_event->sourceid = static_cast<x11::Input::DeviceId>(sourceid);
}

void ScopedXI2Event::InitGenericButtonEvent(int deviceid,
                                            EventType type,
                                            const gfx::Point& location,
                                            int flags) {
  event_ =
      CreateXInput2Event(deviceid, XIButtonEventType(type), 0, gfx::Point());

  auto* dev_event = event_.As<x11::Input::DeviceEvent>();
  dev_event->mods.effective = static_cast<uint32_t>(XEventState(flags));
  dev_event->detail = XButtonEventButton(type, flags);
  dev_event->event_x = ToFp1616(location.x()),
  dev_event->event_y = ToFp1616(location.y()),
  SetXinputMask(dev_event->button_mask.data(), XButtonEventButton(type, flags));

  // Setup an empty valuator list for generic button events.
  SetUpValuators(std::vector<Valuator>());
}

void ScopedXI2Event::InitGenericMouseWheelEvent(int deviceid,
                                                int wheel_delta,
                                                int flags) {
  InitGenericButtonEvent(deviceid, ui::EventType::kMousewheel, gfx::Point(),
                         flags);
  event_.As<x11::Input::DeviceEvent>()->detail = wheel_delta > 0 ? 4 : 5;
}

void ScopedXI2Event::InitScrollEvent(int deviceid,
                                     int x_offset,
                                     int y_offset,
                                     int x_offset_ordinal,
                                     int y_offset_ordinal,
                                     int finger_count) {
  event_ = CreateXInput2Event(deviceid, x11::Input::DeviceEvent::Motion, 0,
                              gfx::Point());

  Valuator valuators[] = {
      Valuator(DeviceDataManagerX11::DT_CMT_SCROLL_X, x_offset),
      Valuator(DeviceDataManagerX11::DT_CMT_SCROLL_Y, y_offset),
      Valuator(DeviceDataManagerX11::DT_CMT_ORDINAL_X, x_offset_ordinal),
      Valuator(DeviceDataManagerX11::DT_CMT_ORDINAL_Y, y_offset_ordinal),
      Valuator(DeviceDataManagerX11::DT_CMT_FINGER_COUNT, finger_count)};
  SetUpValuators(
      std::vector<Valuator>(valuators, valuators + std::size(valuators)));
}

void ScopedXI2Event::InitFlingScrollEvent(int deviceid,
                                          int x_velocity,
                                          int y_velocity,
                                          int x_velocity_ordinal,
                                          int y_velocity_ordinal,
                                          bool is_cancel) {
  event_ = CreateXInput2Event(deviceid, x11::Input::DeviceEvent::Motion,
                              deviceid, gfx::Point());

  Valuator valuators[] = {
      Valuator(DeviceDataManagerX11::DT_CMT_FLING_STATE, is_cancel ? 1 : 0),
      Valuator(DeviceDataManagerX11::DT_CMT_FLING_Y, y_velocity),
      Valuator(DeviceDataManagerX11::DT_CMT_ORDINAL_Y, y_velocity_ordinal),
      Valuator(DeviceDataManagerX11::DT_CMT_FLING_X, x_velocity),
      Valuator(DeviceDataManagerX11::DT_CMT_ORDINAL_X, x_velocity_ordinal)};

  SetUpValuators(
      std::vector<Valuator>(valuators, valuators + std::size(valuators)));
}

void ScopedXI2Event::InitTouchEvent(int deviceid,
                                    int evtype,
                                    int tracking_id,
                                    const gfx::Point& location,
                                    const std::vector<Valuator>& valuators) {
  event_ = CreateXInput2Event(deviceid, evtype, tracking_id, location);

  // If a timestamp was specified, setup the event.
  for (size_t i = 0; i < valuators.size(); ++i) {
    if (valuators[i].data_type ==
        DeviceDataManagerX11::DT_TOUCH_RAW_TIMESTAMP) {
      SetUpValuators(valuators);
      return;
    }
  }

  // No timestamp was specified. Use |ui::EventTimeForNow()|.
  std::vector<Valuator> valuators_with_time = valuators;
  valuators_with_time.emplace_back(
      DeviceDataManagerX11::DT_TOUCH_RAW_TIMESTAMP,
      (ui::EventTimeForNow() - base::TimeTicks()).InMicroseconds());
  SetUpValuators(valuators_with_time);
}

void ScopedXI2Event::SetUpValuators(const std::vector<Valuator>& valuators) {
  auto* devev = event_.As<x11::Input::DeviceEvent>();
  CHECK(devev);
  InitValuatorsForXIDeviceEvent(devev);
  ui::DeviceDataManagerX11* manager = ui::DeviceDataManagerX11::GetInstance();
  for (auto valuator : valuators)
    manager->SetValuatorDataForTest(devev, valuator.data_type, valuator.value);
}

void SetUpTouchPadForTest(int deviceid) {
  std::vector<int> device_list;
  device_list.push_back(deviceid);

  TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);
  ui::DeviceDataManagerX11* manager = ui::DeviceDataManagerX11::GetInstance();
  manager->SetDeviceListForTest(std::vector<int>(), device_list,
                                std::vector<int>());
}

void SetUpTouchDevicesForTest(const std::vector<int>& devices) {
  TouchFactory::GetInstance()->SetTouchDeviceForTest(devices);
  ui::DeviceDataManagerX11* manager = ui::DeviceDataManagerX11::GetInstance();
  manager->SetDeviceListForTest(devices, std::vector<int>(),
                                std::vector<int>());
}

void SetUpPointerDevicesForTest(const std::vector<int>& devices) {
  TouchFactory::GetInstance()->SetPointerDeviceForTest(devices);
  ui::DeviceDataManagerX11* manager = ui::DeviceDataManagerX11::GetInstance();
  manager->SetDeviceListForTest(std::vector<int>(), std::vector<int>(),
                                devices);
}

}  // namespace ui

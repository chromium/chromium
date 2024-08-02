// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/events_x_utils.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/devices/x11/xinput_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/pointer_details.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/xproto.h"

namespace {

// Scroll amount for each wheelscroll event.  120 is what Chrome uses on
// Windows, Fuchsia WHEEL_DELTA, and also it roughly matches Firefox on Linux.
// See https://crbug.com/1270089 for the detailed reasoning.
const int kWheelScrollAmount = 120;

const int kMinWheelButton = 4;
const int kMaxWheelButton = 7;

// A class to track current modifier state on master device. Only track ctrl,
// alt, shift and caps lock keys currently. The tracked state can then be used
// by floating device.
class XModifierStateWatcher {
 public:
  static XModifierStateWatcher* GetInstance() {
    return base::Singleton<XModifierStateWatcher>::get();
  }

  XModifierStateWatcher(const XModifierStateWatcher&) = delete;
  XModifierStateWatcher& operator=(const XModifierStateWatcher&) = delete;

  x11::KeyButMask StateFromKeyboardCode(ui::KeyboardCode keyboard_code) {
    switch (keyboard_code) {
      case ui::VKEY_CONTROL:
        return x11::KeyButMask::Control;
      case ui::VKEY_SHIFT:
        return x11::KeyButMask::Shift;
      case ui::VKEY_MENU:
        return x11::KeyButMask::Mod1;
      case ui::VKEY_CAPITAL:
        return x11::KeyButMask::Lock;
      default:
        return {};
    }
  }

  void UpdateStateFromXEvent(const x11::Event& xev) {
    ui::KeyboardCode keyboard_code = ui::KeyboardCodeFromXKeyEvent(xev);
    auto mask = static_cast<int>(StateFromKeyboardCode(keyboard_code));
    // Floating device can't access the modifier state from master device.
    // We need to track the states of modifier keys in a singleton for
    // floating devices such as touch screen. Issue 106426 is one example
    // of why we need the modifier states for floating device.
    if (auto* key = xev.As<x11::KeyEvent>()) {
      if (key->opcode == x11::KeyEvent::Press)
        state_ = static_cast<int>(key->state) | mask;
      else
        state_ = static_cast<int>(key->state) & ~mask;
    } else if (auto* device = xev.As<x11::Input::DeviceEvent>()) {
      if (device->opcode == x11::Input::DeviceEvent::KeyPress)
        state_ = device->mods.effective | mask;
      else if (device->opcode == x11::Input::DeviceEvent::KeyPress)
        state_ = device->mods.effective & ~mask;
    }
  }

  // Returns the current modifier state in master device. It only contains the
  // state of ctrl, shift, alt and caps lock keys.
  unsigned int state() { return state_; }

 private:
  friend struct base::DefaultSingletonTraits<XModifierStateWatcher>;

  XModifierStateWatcher() = default;

  unsigned int state_{};
};

// Detects if a touch event is a driver-generated 'special event'.
// A 'special event' is a touch event with maximum radius and pressure at
// location (0, 0).
// This needs to be done in a cleaner way: http://crbug.com/169256
bool TouchEventIsGeneratedHack(const x11::Event& x11_event) {
  auto* event = x11_event.As<x11::Input::DeviceEvent>();
  CHECK(event);
  CHECK(event->opcode == x11::Input::DeviceEvent::TouchBegin ||
        event->opcode == x11::Input::DeviceEvent::TouchUpdate ||
        event->opcode == x11::Input::DeviceEvent::TouchEnd);

  // Force is normalized to [0, 1].
  if (ui::GetTouchForceFromXEvent(x11_event) < 1.0f)
    return false;

  if (ui::EventLocationFromXEvent(x11_event) != gfx::Point())
    return false;

  // Radius is in pixels, and the valuator is the diameter in pixels.
  double radius = ui::GetTouchRadiusXFromXEvent(x11_event), min, max;
  auto deviceid = event->sourceid;
  if (!ui::DeviceDataManagerX11::GetInstance()->GetDataRange(
          deviceid, ui::DeviceDataManagerX11::DT_TOUCH_MAJOR, &min, &max)) {
    return false;
  }

  return radius * 2 == max;
}

int GetEventFlagsFromXState(x11::KeyButMask state) {
  int flags = 0;
  if (static_cast<bool>(state & x11::KeyButMask::Shift))
    flags |= ui::EF_SHIFT_DOWN;
  if (static_cast<bool>(state & x11::KeyButMask::Lock))
    flags |= ui::EF_CAPS_LOCK_ON;
  if (static_cast<bool>(state & x11::KeyButMask::Control))
    flags |= ui::EF_CONTROL_DOWN;
  if (static_cast<bool>(state & x11::KeyButMask::Mod1))
    flags |= ui::EF_ALT_DOWN;
  if (static_cast<bool>(state & x11::KeyButMask::Mod2))
    flags |= ui::EF_NUM_LOCK_ON;
  if (static_cast<bool>(state & x11::KeyButMask::Mod3))
    flags |= ui::EF_MOD3_DOWN;
  if (static_cast<bool>(state & x11::KeyButMask::Mod4))
    flags |= ui::EF_COMMAND_DOWN;
  if (static_cast<bool>(state & x11::KeyButMask::Mod5))
    flags |= ui::EF_ALTGR_DOWN;
  if (static_cast<bool>(state & x11::KeyButMask::Button1))
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if (static_cast<bool>(state & x11::KeyButMask::Button2))
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if (static_cast<bool>(state & x11::KeyButMask::Button3))
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  // There are no masks for EF_BACK_MOUSE_BUTTON and
  // EF_FORWARD_MOUSE_BUTTON.
  return flags;
}

int GetEventFlagsFromXState(uint32_t state) {
  return GetEventFlagsFromXState(static_cast<x11::KeyButMask>(state));
}

int GetEventFlagsFromXGenericEvent(const x11::Event& x11_event) {
  auto* xievent = x11_event.As<x11::Input::DeviceEvent>();
  DCHECK(xievent);
  DCHECK(xievent->opcode == x11::Input::DeviceEvent::KeyPress ||
         xievent->opcode == x11::Input::DeviceEvent::KeyRelease);
  return GetEventFlagsFromXState(xievent->mods.effective) |
         (x11_event.send_event() ? ui::EF_FINAL : 0);
}

// Get the event flag for the button in XButtonEvent. During a ButtonPress
// event, |state| in XButtonEvent does not include the button that has just been
// pressed. Instead |state| contains flags for the buttons (if any) that had
// already been pressed before the current button, and |button| stores the most
// current pressed button. So, if you press down left mouse button, and while
// pressing it down, press down the right mouse button, then for the latter
// event, |state| would have Button1Mask set but not Button3Mask, and |button|
// would be 3.
int GetEventFlagsForButton(int button) {
  switch (button) {
    case 1:
      return ui::EF_LEFT_MOUSE_BUTTON;
    case 2:
      return ui::EF_MIDDLE_MOUSE_BUTTON;
    case 3:
      return ui::EF_RIGHT_MOUSE_BUTTON;
    case 8:
      return ui::EF_BACK_MOUSE_BUTTON;
    case 9:
      return ui::EF_FORWARD_MOUSE_BUTTON;
    default:
      return 0;
  }
}

int GetEventFlagsForButton(x11::Button button) {
  return GetEventFlagsForButton(static_cast<int>(button));
}

int GetButtonMaskForX2Event(const x11::Input::DeviceEvent& xievent) {
  int buttonflags = 0;
  for (size_t i = 0; i < 32 * xievent.button_mask.size(); i++) {
    if (ui::IsXinputMaskSet(xievent.button_mask.data(), i)) {
      int button =
          (xievent.sourceid == xievent.deviceid)
              ? ui::DeviceDataManagerX11::GetInstance()->GetMappedButton(i)
              : i;
      buttonflags |= GetEventFlagsForButton(button);
    }
  }
  return buttonflags;
}

ui::EventType GetTouchEventType(const x11::Event& x11_event) {
  auto* event = x11_event.As<x11::Input::DeviceEvent>();
  if (!event) {
    // This is either a crossing event (which are handled by
    // PlatformEventDispatcher directly) or a device changed event (which can
    // happen when --touch-devices flag is used).
    return ui::EventType::kUnknown;
  }
  switch (event->opcode) {
    case x11::Input::DeviceEvent::TouchBegin:
      return TouchEventIsGeneratedHack(x11_event)
                 ? ui::EventType::kUnknown
                 : ui::EventType::kTouchPressed;
    case x11::Input::DeviceEvent::TouchUpdate:
      return TouchEventIsGeneratedHack(x11_event) ? ui::EventType::kUnknown
                                                  : ui::EventType::kTouchMoved;
    case x11::Input::DeviceEvent::TouchEnd:
      return TouchEventIsGeneratedHack(x11_event)
                 ? ui::EventType::kTouchCancelled
                 : ui::EventType::kTouchReleased;
    default:;
  }

  DCHECK(ui::TouchFactory::GetInstance()->IsTouchDevice(event->sourceid));
  switch (event->opcode) {
    case x11::Input::DeviceEvent::ButtonPress:
      return ui::EventType::kTouchPressed;
    case x11::Input::DeviceEvent::ButtonRelease:
      return ui::EventType::kTouchReleased;
    case x11::Input::DeviceEvent::Motion:
      // Should not convert any emulated Motion event from touch device to
      // touch event.
      if (!static_cast<bool>(event->flags &
                             x11::Input::KeyEventFlags::KeyRepeat) &&
          GetButtonMaskForX2Event(*event))
        return ui::EventType::kTouchMoved;
      return ui::EventType::kUnknown;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return ui::EventType::kUnknown;
}

double GetParamFromXEvent(const x11::Event& xev,
                          ui::DeviceDataManagerX11::DataType val,
                          double default_value) {
  ui::DeviceDataManagerX11::GetInstance()->GetEventData(xev, val,
                                                        &default_value);
  return default_value;
}

void ScaleTouchRadius(const x11::Event& x11_event, double* radius) {
  auto* xiev = x11_event.As<x11::Input::DeviceEvent>();
  DCHECK(xiev);
  ui::DeviceDataManagerX11::GetInstance()->ApplyTouchRadiusScale(
      static_cast<uint16_t>(xiev->sourceid), radius);
}

bool GetGestureTimes(const x11::Event& xev,
                     double* start_time,
                     double* end_time) {
  if (!ui::DeviceDataManagerX11::GetInstance()->HasGestureTimes(xev))
    return false;

  double start_time_, end_time_;
  if (!start_time)
    start_time = &start_time_;
  if (!end_time)
    end_time = &end_time_;

  ui::DeviceDataManagerX11::GetInstance()->GetGestureTimes(xev, start_time,
                                                           end_time);
  return true;
}

int64_t g_last_seen_timestamp_ms = 0;
int64_t g_rollover_ms = 0;

// Takes Xlib Time and returns a time delta that is immune to timer rollover.
// This function is not thread safe as we do not use a lock.
base::TimeTicks TimeTicksFromXEventTime(x11::Time timestamp) {
  uint32_t timestamp32 = static_cast<uint32_t>(timestamp);
  int64_t timestamp64 = static_cast<int64_t>(timestamp);

  if (!timestamp64)
    return ui::EventTimeForNow();

  // If this is the first event that we get, assume the time stamp roll-over
  // might have happened before the process was started.
  // Register a rollover if the distance between last timestamp and current one
  // is larger than half the width. This avoids false rollovers even in a case
  // where X server delivers reasonably close events out-of-order.
  bool had_recent_rollover =
      !g_last_seen_timestamp_ms ||
      g_last_seen_timestamp_ms - timestamp64 > (UINT32_MAX >> 1);

  g_last_seen_timestamp_ms = timestamp64;
  if (!had_recent_rollover)
    return base::TimeTicks() + base::Milliseconds(g_rollover_ms + timestamp32);

  DCHECK(timestamp64 <= UINT32_MAX)
      << "X11 Time does not roll over 32 bit, the below logic is likely wrong";

  base::TimeTicks now_ticks = ui::EventTimeForNow();
  int64_t now_ms = (now_ticks - base::TimeTicks()).InMilliseconds();

  g_rollover_ms = now_ms & ~static_cast<int64_t>(UINT32_MAX);
  uint32_t delta = static_cast<uint32_t>(now_ms - timestamp32);
  return base::TimeTicks() + base::Milliseconds(now_ms - delta);
}

base::TimeTicks TimeTicksFromXEvent(const x11::Event& xev) {
  if (auto* key = xev.As<x11::KeyEvent>())
    return TimeTicksFromXEventTime(key->time);
  if (auto* button = xev.As<x11::ButtonEvent>())
    return TimeTicksFromXEventTime(button->time);
  if (auto* motion = xev.As<x11::MotionNotifyEvent>())
    return TimeTicksFromXEventTime(motion->time);
  if (auto* crossing = xev.As<x11::CrossingEvent>())
    return TimeTicksFromXEventTime(crossing->time);
  if (auto* device = xev.As<x11::Input::DeviceEvent>()) {
    double start, end;
    double touch_timestamp;
    if (GetGestureTimes(xev, &start, &end)) {
      // If the driver supports gesture times, use them.
      return ui::EventTimeStampFromSeconds(end);
    } else if (ui::DeviceDataManagerX11::GetInstance()->GetEventData(
                   xev, ui::DeviceDataManagerX11::DT_TOUCH_RAW_TIMESTAMP,
                   &touch_timestamp)) {
      return ui::EventTimeStampFromSeconds(touch_timestamp);
    }
    return TimeTicksFromXEventTime(device->time);
  }
  NOTREACHED_IN_MIGRATION();
  return base::TimeTicks();
}

// This is ported from libxi's FP1616toDBL in XExtInt.c
double Fp1616ToDouble(x11::Input::Fp1616 x) {
  auto x32 = static_cast<int32_t>(x);
  return x32 * 1.0 / (1 << 16);
}

}  // namespace

namespace ui {

EventType EventTypeFromXEvent(const x11::Event& xev) {
  // Allow the DeviceDataManager to block the event. If blocked return
  // EventType::kUnknown as the type so this event will not be further
  // processed. NOTE: During some events unittests there is no device data
  // manager.
  if (DeviceDataManager::HasInstance() &&
      DeviceDataManagerX11::GetInstance()->IsEventBlocked(xev)) {
    return EventType::kUnknown;
  }

  if (auto* key = xev.As<x11::KeyEvent>()) {
    return key->opcode == x11::KeyEvent::Press ? EventType::kKeyPressed
                                               : EventType::kKeyReleased;
  }
  if (auto* xbutton = xev.As<x11::ButtonEvent>()) {
    int button = static_cast<int>(xbutton->detail);
    bool wheel = button >= kMinWheelButton && button <= kMaxWheelButton;
    if (xbutton->opcode == x11::ButtonEvent::Press) {
      return wheel ? EventType::kMousewheel : EventType::kMousePressed;
    }
    // Drop wheel events; we should've already scrolled on the press.
    return wheel ? EventType::kUnknown : EventType::kMouseReleased;
  }
  if (auto* motion = xev.As<x11::MotionNotifyEvent>()) {
    bool primary_button = static_cast<bool>(
        motion->state & (x11::KeyButMask::Button1 | x11::KeyButMask::Button2 |
                         x11::KeyButMask::Button3));
    return primary_button ? EventType::kMouseDragged : EventType::kMouseMoved;
  }
  if (auto* crossing = xev.As<x11::CrossingEvent>()) {
    bool enter = crossing->opcode == x11::CrossingEvent::EnterNotify;
    // The standard on Windows is to send a MouseMove event when the mouse
    // first enters a window instead of sending a special mouse enter event.
    // To be consistent we follow the same style.
    return enter ? EventType::kMouseMoved : EventType::kMouseExited;
  }
  if (auto* xievent = xev.As<x11::Input::DeviceEvent>()) {
    TouchFactory* factory = TouchFactory::GetInstance();
    if (!factory->ShouldProcessDeviceEvent(*xievent))
      return EventType::kUnknown;

    // This check works only for master and floating slave devices. That is
    // why it is necessary to check for the Touch events in the following
    // switch statement to account for attached-slave touchscreens.
    if (factory->IsTouchDevice(xievent->sourceid))
      return GetTouchEventType(xev);

    switch (xievent->opcode) {
      case x11::Input::DeviceEvent::TouchBegin:
        return ui::EventType::kTouchPressed;
      case x11::Input::DeviceEvent::TouchUpdate:
        return ui::EventType::kTouchMoved;
      case x11::Input::DeviceEvent::TouchEnd:
        return ui::EventType::kTouchReleased;
      case x11::Input::DeviceEvent::ButtonPress: {
        int button = EventButtonFromXEvent(xev);
        if (button >= kMinWheelButton && button <= kMaxWheelButton)
          return EventType::kMousewheel;
        return EventType::kMousePressed;
      }
      case x11::Input::DeviceEvent::ButtonRelease: {
        int button = EventButtonFromXEvent(xev);
        // Drop wheel events; we should've already scrolled on the press.
        if (button >= kMinWheelButton && button <= kMaxWheelButton)
          return EventType::kUnknown;
        return EventType::kMouseReleased;
      }
      case x11::Input::DeviceEvent::Motion: {
        bool is_cancel;
        DeviceDataManagerX11* devices = DeviceDataManagerX11::GetInstance();
        if (GetFlingDataFromXEvent(xev, nullptr, nullptr, nullptr, nullptr,
                                   &is_cancel))
          return is_cancel ? EventType::kScrollFlingCancel
                           : EventType::kScrollFlingStart;
        if (devices->IsScrollEvent(xev)) {
          return devices->IsTouchpadXInputEvent(xev) ? EventType::kScroll
                                                     : EventType::kMousewheel;
        }
        if (devices->GetScrollClassEventDetail(xev) != SCROLL_TYPE_NO_SCROLL) {
          return devices->IsTouchpadXInputEvent(xev) ? EventType::kScroll
                                                     : EventType::kMousewheel;
        }
        if (devices->IsCMTMetricsEvent(xev))
          return EventType::kUmaData;
        if (GetButtonMaskForX2Event(*xievent))
          return EventType::kMouseDragged;
        if (DeviceDataManagerX11::GetInstance()->HasEventData(
                xev, DeviceDataManagerX11::DT_CMT_SCROLL_X) ||
            DeviceDataManagerX11::GetInstance()->HasEventData(
                xev, DeviceDataManagerX11::DT_CMT_SCROLL_Y)) {
          // Don't produce mouse move events for mousewheel scrolls.
          return EventType::kUnknown;
        }

        return EventType::kMouseMoved;
      }
      case x11::Input::DeviceEvent::KeyPress:
        return EventType::kKeyPressed;
      case x11::Input::DeviceEvent::KeyRelease:
        return EventType::kKeyReleased;
    }
  }
  return EventType::kUnknown;
}

int GetEventFlagsFromXKeyEvent(const x11::KeyEvent& key, bool send_event) {
  const auto state = static_cast<int>(key.state);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const int ime_fabricated_flag = 0;
#else
  // XIM fabricates key events for the character compositions by XK_Multi_key.
  // For example, when a user hits XK_Multi_key, XK_apostrophe, and XK_e in
  // order to input "é", then XIM generates a key event with keycode=0 and
  // state=0 for the composition, and the sequence of X11 key events will be
  // XK_Multi_key, XK_apostrophe, **NoSymbol**, and XK_e.  If the user used
  // shift key and/or caps lock key, state can be ShiftMask, LockMask or both.
  //
  // We have to send these fabricated key events to XIM so it can correctly
  // handle the character compositions.
  const auto detail = static_cast<uint8_t>(key.detail);
  const auto shift_lock_mask =
      static_cast<int>(x11::KeyButMask::Shift | x11::KeyButMask::Lock);
  const bool fabricated_by_xim = detail == 0 && (state & ~shift_lock_mask) == 0;
  const int ime_fabricated_flag =
      fabricated_by_xim ? ui::EF_IME_FABRICATED_KEY : 0;
#endif

  return GetEventFlagsFromXState(state) | (send_event ? ui::EF_FINAL : 0) |
         ime_fabricated_flag;
}

int EventFlagsFromXEvent(const x11::Event& xev) {
  if (auto* key = xev.As<x11::KeyEvent>()) {
    XModifierStateWatcher::GetInstance()->UpdateStateFromXEvent(xev);
    return GetEventFlagsFromXKeyEvent(*key, xev.send_event());
  }
  if (auto* button = xev.As<x11::ButtonEvent>()) {
    int flags = GetEventFlagsFromXState(button->state);
    const EventType type = EventTypeFromXEvent(xev);
    if (type == EventType::kMousePressed || type == EventType::kMouseReleased) {
      flags |= GetEventFlagsForButton(button->detail);
    }
    return flags;
  }
  if (auto* crossing = xev.As<x11::CrossingEvent>()) {
    int state = GetEventFlagsFromXState(crossing->state);
    // EnterNotify creates EventType::kMouseMoved. Mark as synthesized as this
    // is not a real mouse move event.
    if (crossing->opcode == x11::CrossingEvent::EnterNotify)
      state |= EF_IS_SYNTHESIZED;
    return state;
  }
  if (auto* motion = xev.As<x11::MotionNotifyEvent>())
    return GetEventFlagsFromXState(motion->state);
  if (auto* xievent = xev.As<x11::Input::DeviceEvent>()) {
    switch (xievent->opcode) {
      case x11::Input::DeviceEvent::TouchBegin:
      case x11::Input::DeviceEvent::TouchUpdate:
      case x11::Input::DeviceEvent::TouchEnd:
        return GetButtonMaskForX2Event(*xievent) |
               GetEventFlagsFromXState(xievent->mods.effective) |
               GetEventFlagsFromXState(
                   XModifierStateWatcher::GetInstance()->state());
      case x11::Input::DeviceEvent::ButtonPress:
      case x11::Input::DeviceEvent::ButtonRelease: {
        const bool touch =
            TouchFactory::GetInstance()->IsTouchDevice(xievent->sourceid);
        int flags = GetButtonMaskForX2Event(*xievent) |
                    GetEventFlagsFromXState(xievent->mods.effective);
        if (touch) {
          flags |= GetEventFlagsFromXState(
              XModifierStateWatcher::GetInstance()->state());
        }

        const EventType type = EventTypeFromXEvent(xev);
        int button = EventButtonFromXEvent(xev);
        if ((type == EventType::kMousePressed ||
             type == EventType::kMouseReleased) &&
            !touch) {
          flags |= GetEventFlagsForButton(button);
        }
        return flags;
      }
      case x11::Input::DeviceEvent::Motion:
        return GetButtonMaskForX2Event(*xievent) |
               GetEventFlagsFromXState(xievent->mods.effective);
      case x11::Input::DeviceEvent::KeyPress:
      case x11::Input::DeviceEvent::KeyRelease: {
        XModifierStateWatcher::GetInstance()->UpdateStateFromXEvent(xev);
        return GetEventFlagsFromXGenericEvent(xev);
      }
    }
  }
  return 0;
}

base::TimeTicks EventTimeFromXEvent(const x11::Event& xev) {
  auto timestamp = TimeTicksFromXEvent(xev);
  ValidateEventTimeClock(&timestamp);
  return timestamp;
}

gfx::Point EventLocationFromXEvent(const x11::Event& xev) {
  if (auto* crossing = xev.As<x11::CrossingEvent>())
    return gfx::Point(crossing->event_x, crossing->event_y);
  if (auto* button = xev.As<x11::ButtonEvent>())
    return gfx::Point(button->event_x, button->event_y);
  if (auto* motion = xev.As<x11::MotionNotifyEvent>())
    return gfx::Point(motion->event_x, motion->event_y);
  if (auto* xievent = xev.As<x11::Input::DeviceEvent>()) {
    float x = Fp1616ToDouble(xievent->event_x);
    float y = Fp1616ToDouble(xievent->event_y);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    switch (xievent->opcode) {
      case x11::Input::DeviceEvent::TouchBegin:
      case x11::Input::DeviceEvent::TouchUpdate:
      case x11::Input::DeviceEvent::TouchEnd:
        ui::DeviceDataManagerX11::GetInstance()->ApplyTouchTransformer(
            static_cast<uint16_t>(xievent->deviceid), &x, &y);
        break;
      default:
        break;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return gfx::Point(static_cast<int>(x), static_cast<int>(y));
  }
  return gfx::Point();
}

gfx::Point EventSystemLocationFromXEvent(const x11::Event& xev) {
  if (auto* crossing = xev.As<x11::CrossingEvent>())
    return gfx::Point(crossing->root_x, crossing->root_y);
  if (auto* button = xev.As<x11::ButtonEvent>())
    return gfx::Point(button->root_x, button->root_y);
  if (auto* motion = xev.As<x11::MotionNotifyEvent>())
    return gfx::Point(motion->root_x, motion->root_y);
  if (auto* crossing = xev.As<x11::Input::CrossingEvent>()) {
    return gfx::Point(Fp1616ToDouble(crossing->root_x),
                      Fp1616ToDouble(crossing->root_y));
  }
  if (auto* xievent = xev.As<x11::Input::DeviceEvent>()) {
    return gfx::Point(Fp1616ToDouble(xievent->root_x),
                      Fp1616ToDouble(xievent->root_y));
  }
  return gfx::Point();
}

int EventButtonFromXEvent(const x11::Event& xev) {
  auto* xievent = xev.As<x11::Input::DeviceEvent>();
  DCHECK(xievent);
  int button = xievent->detail;

  return (xievent->sourceid == xievent->deviceid)
             ? DeviceDataManagerX11::GetInstance()->GetMappedButton(button)
             : button;
}

int GetChangedMouseButtonFlagsFromXEvent(const x11::Event& xev) {
  if (auto* button = xev.As<x11::ButtonEvent>())
    return GetEventFlagsForButton(button->detail);
  auto* device = xev.As<x11::Input::DeviceEvent>();
  if (device && (device->opcode == x11::Input::DeviceEvent::ButtonPress ||
                 device->opcode == x11::Input::DeviceEvent::ButtonRelease)) {
    return GetEventFlagsForButton(EventButtonFromXEvent(xev));
  }
  return 0;
}

gfx::Vector2d GetMouseWheelOffsetFromXEvent(const x11::Event& xev) {
  float x_offset, y_offset;
  if (GetScrollOffsetsFromXEvent(xev, &x_offset, &y_offset, nullptr, nullptr,
                                 nullptr)) {
    return gfx::Vector2d(static_cast<int>(x_offset),
                         static_cast<int>(y_offset));
  }

  auto* device = xev.As<x11::Input::DeviceEvent>();
  int button = device ? EventButtonFromXEvent(xev)
                      : static_cast<int>(xev.As<x11::ButtonEvent>()->detail);

  // If this is an xinput1 scroll event from an xinput2 mouse then we need to
  // block the legacy scroll events for the necessary axes.
  int scroll_class_type =
      DeviceDataManagerX11::GetInstance()->GetScrollClassDeviceDetail(xev);
  bool xi2_vertical = scroll_class_type & SCROLL_TYPE_VERTICAL;
  bool xi2_horizontal = scroll_class_type & SCROLL_TYPE_HORIZONTAL;

  switch (button) {
    case 4:
      return gfx::Vector2d(0, xi2_vertical ? 0 : kWheelScrollAmount);
    case 5:
      return gfx::Vector2d(0, xi2_vertical ? 0 : -kWheelScrollAmount);
    case 6:
      return gfx::Vector2d(xi2_horizontal ? 0 : kWheelScrollAmount, 0);
    case 7:
      return gfx::Vector2d(xi2_horizontal ? 0 : -kWheelScrollAmount, 0);
    default:
      return gfx::Vector2d();
  }
}

float GetStylusForceFromXEvent(const x11::Event& x11_event) {
  auto* event = x11_event.As<x11::Input::DeviceEvent>();
  if (event->opcode == x11::Input::DeviceEvent::ButtonRelease)
    return 0.0;
  double force = GetParamFromXEvent(
      x11_event, ui::DeviceDataManagerX11::DT_STYLUS_PRESSURE, 0.0);
  auto deviceid = event->sourceid;
  // Force is normalized to fall into [0, 1]
  if (!ui::DeviceDataManagerX11::GetInstance()->NormalizeData(
          deviceid, ui::DeviceDataManagerX11::DT_STYLUS_PRESSURE, &force)) {
    force = 0.0;
  }
  return force;
}

float GetStylusTiltXFromXEvent(const x11::Event& x11_event) {
  double tilt = GetParamFromXEvent(
      x11_event, ui::DeviceDataManagerX11::DT_STYLUS_TILT_X, 0.0);
  return std::clamp<float>(tilt, -90, 90);
}

float GetStylusTiltYFromXEvent(const x11::Event& x11_event) {
  double tilt = GetParamFromXEvent(
      x11_event, ui::DeviceDataManagerX11::DT_STYLUS_TILT_Y, 0.0);
  return std::clamp<float>(tilt, -90, 90);
}

PointerDetails GetStylusPointerDetailsFromXEvent(const x11::Event& xev) {
  if (!ui::DeviceDataManagerX11::HasInstance() ||
      !ui::DeviceDataManagerX11::GetInstance()->IsStylusXInputEvent(xev)) {
    // default: empty details with kMouse
    return PointerDetails(EventPointerType::kMouse);
  }
  PointerDetails p(EventPointerType::kPen);
  // NOTE: id is not set here
  p.force = GetStylusForceFromXEvent(xev);
  p.tilt_x = GetStylusTiltXFromXEvent(xev);
  p.tilt_y = GetStylusTiltYFromXEvent(xev);
  return p;
}

int GetTouchIdFromXEvent(const x11::Event& xev) {
  double slot = 0;
  ui::DeviceDataManagerX11* manager = ui::DeviceDataManagerX11::GetInstance();
  double tracking_id;
  if (!manager->GetEventData(
          xev, ui::DeviceDataManagerX11::DT_TOUCH_TRACKING_ID, &tracking_id)) {
    LOG(ERROR) << "Could not get the tracking ID for the event. Using 0.";
  } else {
    ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
    slot = factory->GetSlotForTrackingID(tracking_id);
  }
  return slot;
}

float GetTouchRadiusXFromXEvent(const x11::Event& xev) {
  double radius =
      GetParamFromXEvent(xev, ui::DeviceDataManagerX11::DT_TOUCH_MAJOR, 0.0) /
      2.0;
  ScaleTouchRadius(xev, &radius);
  return radius;
}

float GetTouchRadiusYFromXEvent(const x11::Event& xev) {
  double radius =
      GetParamFromXEvent(xev, ui::DeviceDataManagerX11::DT_TOUCH_MINOR, 0.0) /
      2.0;
  ScaleTouchRadius(xev, &radius);
  return radius;
}

float GetTouchAngleFromXEvent(const x11::Event& xev) {
  return GetParamFromXEvent(xev, ui::DeviceDataManagerX11::DT_TOUCH_ORIENTATION,
                            0.0) /
         2.0;
}

float GetTouchForceFromXEvent(const x11::Event& x11_event) {
  auto* event = x11_event.As<x11::Input::DeviceEvent>();
  if (event->opcode == x11::Input::DeviceEvent::TouchEnd)
    return 0.0;
  double force = 0.0;
  force = GetParamFromXEvent(x11_event,
                             ui::DeviceDataManagerX11::DT_TOUCH_PRESSURE, 0.0);
  auto deviceid = event->sourceid;
  // Force is normalized to fall into [0, 1]
  if (!ui::DeviceDataManagerX11::GetInstance()->NormalizeData(
          deviceid, ui::DeviceDataManagerX11::DT_TOUCH_PRESSURE, &force))
    force = 0.0;
  return force;
}

PointerDetails GetTouchPointerDetailsFromXEvent(const x11::Event& xev) {
  auto* event = xev.As<x11::Input::DeviceEvent>();

  // Use touch as the default pointer type if `event` is null.
  EventPointerType pointer_type =
      event ? ui::TouchFactory::GetInstance()->GetTouchDevicePointerType(
                  event->sourceid)
            : EventPointerType::kTouch;
  return PointerDetails(
      pointer_type, GetTouchIdFromXEvent(xev), GetTouchRadiusXFromXEvent(xev),
      GetTouchRadiusYFromXEvent(xev), GetTouchForceFromXEvent(xev),
      GetTouchAngleFromXEvent(xev));
}

bool GetScrollOffsetsFromXEvent(const x11::Event& xev,
                                float* x_offset,
                                float* y_offset,
                                float* x_offset_ordinal,
                                float* y_offset_ordinal,
                                int* finger_count) {
  // Temp values to prevent passing nullptrs to DeviceDataManager.
  float x_scroll_offset, y_scroll_offset;
  float x_scroll_offset_ordinal, y_scroll_offset_ordinal;
  int finger;
  if (!x_offset)
    x_offset = &x_scroll_offset;
  if (!y_offset)
    y_offset = &y_scroll_offset;
  if (!x_offset_ordinal)
    x_offset_ordinal = &x_scroll_offset_ordinal;
  if (!y_offset_ordinal)
    y_offset_ordinal = &y_scroll_offset_ordinal;
  if (!finger_count)
    finger_count = &finger;

  if (DeviceDataManagerX11::GetInstance()->IsScrollEvent(xev)) {
    DeviceDataManagerX11::GetInstance()->GetScrollOffsets(
        xev, x_offset, y_offset, x_offset_ordinal, y_offset_ordinal,
        finger_count);
    return true;
  }

  if (DeviceDataManagerX11::GetInstance()->GetScrollClassEventDetail(xev) !=
      SCROLL_TYPE_NO_SCROLL) {
    double x_scroll_offset_dbl, y_scroll_offset_dbl;
    DeviceDataManagerX11::GetInstance()->GetScrollClassOffsets(
        xev, &x_scroll_offset_dbl, &y_scroll_offset_dbl);
    *x_offset = x_scroll_offset_dbl * kWheelScrollAmount;
    *y_offset = y_scroll_offset_dbl * kWheelScrollAmount;

    if (DeviceDataManagerX11::GetInstance()->IsTouchpadXInputEvent(xev)) {
      *x_offset_ordinal = *x_offset;
      *y_offset_ordinal = *y_offset;
      // In libinput, we can check to validate whether the device supports
      // 'two_finger', 'edge' scrolling or not. See
      // https://www.mankier.com/4/libinput.
      *finger_count = 2;
    }
    return true;
  }
  return false;
}

bool GetFlingDataFromXEvent(const x11::Event& xev,
                            float* vx,
                            float* vy,
                            float* vx_ordinal,
                            float* vy_ordinal,
                            bool* is_cancel) {
  if (!DeviceDataManagerX11::GetInstance()->IsFlingEvent(xev))
    return false;

  float vx_, vy_;
  float vx_ordinal_, vy_ordinal_;
  bool is_cancel_;
  if (!vx)
    vx = &vx_;
  if (!vy)
    vy = &vy_;
  if (!vx_ordinal)
    vx_ordinal = &vx_ordinal_;
  if (!vy_ordinal)
    vy_ordinal = &vy_ordinal_;
  if (!is_cancel)
    is_cancel = &is_cancel_;

  DeviceDataManagerX11::GetInstance()->GetFlingData(xev, vx, vy, vx_ordinal,
                                                    vy_ordinal, is_cancel);
  return true;
}

bool IsAltPressed() {
  return XModifierStateWatcher::GetInstance()->state() &
         static_cast<int>(x11::KeyButMask::Mod1);
}

int GetModifierKeyState() {
  return XModifierStateWatcher::GetInstance()->state();
}

void ResetTimestampRolloverCountersForTesting() {
  g_last_seen_timestamp_ms = 0;
  g_rollover_ms = 0;
}

}  // namespace ui

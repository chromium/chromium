// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/events_x_utils.h"

#include <stddef.h>
#include <string.h>
#include <cmath>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace {

// Scroll amount for each wheelscroll event. 53 is also the value used for GTK+.
const int kWheelScrollAmount = 53;

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

  int StateFromKeyboardCode(ui::KeyboardCode keyboard_code) {
    switch (keyboard_code) {
      case ui::VKEY_CONTROL:
        return ControlMask;
      case ui::VKEY_SHIFT:
        return ShiftMask;
      case ui::VKEY_MENU:
        return Mod1Mask;
      case ui::VKEY_CAPITAL:
        return LockMask;
      default:
        return 0;
    }
  }

  void UpdateStateFromXEvent(const XEvent& xev) {
    ui::KeyboardCode keyboard_code = ui::KeyboardCodeFromXKeyEvent(&xev);
    unsigned int mask = StateFromKeyboardCode(keyboard_code);
    // Floating device can't access the modifer state from master device.
    // We need to track the states of modifier keys in a singleton for
    // floating devices such as touch screen. Issue 106426 is one example
    // of why we need the modifier states for floating device.
    switch (xev.type) {
      case KeyPress:
        state_ = xev.xkey.state | mask;
        break;
      case KeyRelease:
        state_ = xev.xkey.state & ~mask;
        break;
      case GenericEvent: {
        XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);
        switch (xievent->evtype) {
          case XI_KeyPress:
            state_ = xievent->mods.effective |= mask;
            break;
          case XI_KeyRelease:
            state_ = xievent->mods.effective &= ~mask;
            break;
          default:
            NOTREACHED();
            break;
        }
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

  // Returns the current modifer state in master device. It only contains the
  // state of ctrl, shift, alt and caps lock keys.
  unsigned int state() { return state_; }

 private:
  friend struct base::DefaultSingletonTraits<XModifierStateWatcher>;

  XModifierStateWatcher() : state_(0) {}

  unsigned int state_;

  DISALLOW_COPY_AND_ASSIGN(XModifierStateWatcher);
};

// Detects if a touch event is a driver-generated 'special event'.
// A 'special event' is a touch event with maximum radius and pressure at
// location (0, 0).
// This needs to be done in a cleaner way: http://crbug.com/169256
bool TouchEventIsGeneratedHack(const XEvent& xev) {
  XIDeviceEvent* event = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  CHECK(event->evtype == XI_TouchBegin || event->evtype == XI_TouchUpdate ||
        event->evtype == XI_TouchEnd);

  // Force is normalized to [0, 1].
  if (ui::GetTouchForceFromXEvent(xev) < 1.0f)
    return false;

  if (ui::EventLocationFromXEvent(xev) != gfx::Point())
    return false;

  // Radius is in pixels, and the valuator is the diameter in pixels.
  double radius = ui::GetTouchRadiusXFromXEvent(xev), min, max;
  unsigned int deviceid =
      static_cast<XIDeviceEvent*>(xev.xcookie.data)->sourceid;
  if (!ui::DeviceDataManagerX11::GetInstance()->GetDataRange(
          deviceid, ui::DeviceDataManagerX11::DT_TOUCH_MAJOR, &min, &max)) {
    return false;
  }

  return radius * 2 == max;
}

int GetEventFlagsFromXState(unsigned int state) {
  int flags = 0;
  if (state & ShiftMask)
    flags |= ui::EF_SHIFT_DOWN;
  if (state & LockMask)
    flags |= ui::EF_CAPS_LOCK_ON;
  if (state & ControlMask)
    flags |= ui::EF_CONTROL_DOWN;
  if (state & Mod1Mask)
    flags |= ui::EF_ALT_DOWN;
  if (state & Mod2Mask)
    flags |= ui::EF_NUM_LOCK_ON;
  if (state & Mod3Mask)
    flags |= ui::EF_MOD3_DOWN;
  if (state & Mod4Mask)
    flags |= ui::EF_COMMAND_DOWN;
  if (state & Mod5Mask)
    flags |= ui::EF_ALTGR_DOWN;
  if (state & Button1Mask)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if (state & Button2Mask)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if (state & Button3Mask)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
  // There are no masks for EF_BACK_MOUSE_BUTTON and
  // EF_FORWARD_MOUSE_BUTTON.
  return flags;
}

int GetEventFlagsFromXKeyEvent(const XEvent& xev) {
  DCHECK(xev.type == KeyPress || xev.type == KeyRelease);

#if defined(OS_CHROMEOS)
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
  const unsigned int shift_lock_mask = ShiftMask | LockMask;
  const bool fabricated_by_xim =
      xev.xkey.keycode == 0 && (xev.xkey.state & ~shift_lock_mask) == 0;
  const int ime_fabricated_flag =
      fabricated_by_xim ? ui::EF_IME_FABRICATED_KEY : 0;
#endif

  return GetEventFlagsFromXState(xev.xkey.state) |
         (xev.xkey.send_event ? ui::EF_FINAL : 0) | ime_fabricated_flag;
}

int GetEventFlagsFromXGenericEvent(const XEvent& xev) {
  DCHECK(xev.type == GenericEvent);
  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  DCHECK((xievent->evtype == XI_KeyPress) ||
         (xievent->evtype == XI_KeyRelease));
  return GetEventFlagsFromXState(xievent->mods.effective) |
         (xev.xkey.send_event ? ui::EF_FINAL : 0);
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

int GetButtonMaskForX2Event(XIDeviceEvent* xievent) {
  int buttonflags = 0;
  for (int i = 0; i < 8 * xievent->buttons.mask_len; i++) {
    if (XIMaskIsSet(xievent->buttons.mask, i)) {
      int button =
          (xievent->sourceid == xievent->deviceid)
              ? ui::DeviceDataManagerX11::GetInstance()->GetMappedButton(i)
              : i;
      buttonflags |= GetEventFlagsForButton(button);
    }
  }
  return buttonflags;
}

ui::EventType GetTouchEventType(const XEvent& xev) {
  XIDeviceEvent* event = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  switch (event->evtype) {
    case XI_TouchBegin:
      return TouchEventIsGeneratedHack(xev) ? ui::ET_UNKNOWN
                                            : ui::ET_TOUCH_PRESSED;
    case XI_TouchUpdate:
      return TouchEventIsGeneratedHack(xev) ? ui::ET_UNKNOWN
                                            : ui::ET_TOUCH_MOVED;
    case XI_TouchEnd:
      return TouchEventIsGeneratedHack(xev) ? ui::ET_TOUCH_CANCELLED
                                            : ui::ET_TOUCH_RELEASED;
  }

  DCHECK(ui::TouchFactory::GetInstance()->IsTouchDevice(event->sourceid));
  switch (event->evtype) {
    case XI_ButtonPress:
      return ui::ET_TOUCH_PRESSED;
    case XI_ButtonRelease:
      return ui::ET_TOUCH_RELEASED;
    case XI_Motion:
      // Should not convert any emulated Motion event from touch device to
      // touch event.
      if (!(event->flags & XIPointerEmulated) && GetButtonMaskForX2Event(event))
        return ui::ET_TOUCH_MOVED;
      return ui::ET_UNKNOWN;
    case XI_DeviceChanged:
      // This can happen when --touch-devices flag is used.
      return ui::ET_UNKNOWN;
    case XI_Leave:
    case XI_Enter:
    case XI_FocusIn:
    case XI_FocusOut:
      // These may be handled by the PlatformEventDispatcher directly.
      return ui::ET_UNKNOWN;
    default:
      NOTREACHED();
  }
  return ui::ET_UNKNOWN;
}

double GetTouchParamFromXEvent(const XEvent& xev,
                               ui::DeviceDataManagerX11::DataType val,
                               double default_value) {
  ui::DeviceDataManagerX11::GetInstance()->GetEventData(xev, val,
                                                        &default_value);
  return default_value;
}

void ScaleTouchRadius(const XEvent& xev, double* radius) {
  DCHECK_EQ(GenericEvent, xev.type);
  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  ui::DeviceDataManagerX11::GetInstance()->ApplyTouchRadiusScale(xiev->sourceid,
                                                                 radius);
}

bool GetGestureTimes(const XEvent& xev, double* start_time, double* end_time) {
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
base::TimeTicks TimeTicksFromXEventTime(Time timestamp) {
  int64_t timestamp64 = timestamp;

  if (!timestamp)
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
    return base::TimeTicks() +
        base::TimeDelta::FromMilliseconds(g_rollover_ms + timestamp);

  DCHECK(timestamp64 <= UINT32_MAX)
      << "X11 Time does not roll over 32 bit, the below logic is likely wrong";

  base::TimeTicks now_ticks = ui::EventTimeForNow();
  int64_t now_ms = (now_ticks - base::TimeTicks()).InMilliseconds();

  g_rollover_ms = now_ms & ~static_cast<int64_t>(UINT32_MAX);
  uint32_t delta = static_cast<uint32_t>(now_ms - timestamp);
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(now_ms - delta);
}

}  // namespace

namespace ui {

EventType EventTypeFromXEvent(const XEvent& xev) {
  // Allow the DeviceDataManager to block the event. If blocked return
  // ET_UNKNOWN as the type so this event will not be further processed.
  // NOTE: During some events unittests there is no device data manager.
  if (DeviceDataManager::HasInstance() &&
      DeviceDataManagerX11::GetInstance()->IsEventBlocked(xev)) {
    return ET_UNKNOWN;
  }

  switch (xev.type) {
    case KeyPress:
      return ET_KEY_PRESSED;
    case KeyRelease:
      return ET_KEY_RELEASED;
    case ButtonPress:
      if (static_cast<int>(xev.xbutton.button) >= kMinWheelButton &&
          static_cast<int>(xev.xbutton.button) <= kMaxWheelButton)
        return ET_MOUSEWHEEL;
      return ET_MOUSE_PRESSED;
    case ButtonRelease:
      // Drop wheel events; we should've already scrolled on the press.
      if (static_cast<int>(xev.xbutton.button) >= kMinWheelButton &&
          static_cast<int>(xev.xbutton.button) <= kMaxWheelButton)
        return ET_UNKNOWN;
      return ET_MOUSE_RELEASED;
    case MotionNotify:
      if (xev.xmotion.state & (Button1Mask | Button2Mask | Button3Mask))
        return ET_MOUSE_DRAGGED;
      return ET_MOUSE_MOVED;
    case EnterNotify:
      // The standard on Windows is to send a MouseMove event when the mouse
      // first enters a window instead of sending a special mouse enter event.
      // To be consistent we follow the same style.
      return ET_MOUSE_MOVED;
    case LeaveNotify:
      return ET_MOUSE_EXITED;
    case GenericEvent: {
      TouchFactory* factory = TouchFactory::GetInstance();
      if (!factory->ShouldProcessXI2Event(const_cast<XEvent*>(&xev)))
        return ET_UNKNOWN;

      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);

      // This check works only for master and floating slave devices. That is
      // why it is necessary to check for the XI_Touch* events in the following
      // switch statement to account for attached-slave touchscreens.
      if (factory->IsTouchDevice(xievent->sourceid))
        return GetTouchEventType(xev);

      switch (xievent->evtype) {
        case XI_TouchBegin:
          return ui::ET_TOUCH_PRESSED;
        case XI_TouchUpdate:
          return ui::ET_TOUCH_MOVED;
        case XI_TouchEnd:
          return ui::ET_TOUCH_RELEASED;
        case XI_ButtonPress: {
          int button = EventButtonFromXEvent(xev);
          if (button >= kMinWheelButton && button <= kMaxWheelButton)
            return ET_MOUSEWHEEL;
          return ET_MOUSE_PRESSED;
        }
        case XI_ButtonRelease: {
          int button = EventButtonFromXEvent(xev);
          // Drop wheel events; we should've already scrolled on the press.
          if (button >= kMinWheelButton && button <= kMaxWheelButton)
            return ET_UNKNOWN;
          return ET_MOUSE_RELEASED;
        }
        case XI_Motion: {
          bool is_cancel;
          DeviceDataManagerX11* devices = DeviceDataManagerX11::GetInstance();
          if (GetFlingDataFromXEvent(xev, NULL, NULL, NULL, NULL, &is_cancel))
            return is_cancel ? ET_SCROLL_FLING_CANCEL : ET_SCROLL_FLING_START;
          if (devices->IsScrollEvent(xev)) {
            return devices->IsTouchpadXInputEvent(xev) ? ET_SCROLL
                                                       : ET_MOUSEWHEEL;
          }
          if (devices->GetScrollClassEventDetail(xev) !=
              SCROLL_TYPE_NO_SCROLL) {
            return devices->IsTouchpadXInputEvent(xev) ? ET_SCROLL
                                                       : ET_MOUSEWHEEL;
          }
          if (devices->IsCMTMetricsEvent(xev))
            return ET_UMA_DATA;
          if (GetButtonMaskForX2Event(xievent))
            return ET_MOUSE_DRAGGED;
          if (DeviceDataManagerX11::GetInstance()->HasEventData(
                  xievent, DeviceDataManagerX11::DT_CMT_SCROLL_X) ||
              DeviceDataManagerX11::GetInstance()->HasEventData(
                  xievent, DeviceDataManagerX11::DT_CMT_SCROLL_Y)) {
            // Don't produce mouse move events for mousewheel scrolls.
            return ET_UNKNOWN;
          }

          return ET_MOUSE_MOVED;
        }
        case XI_KeyPress:
          return ET_KEY_PRESSED;
        case XI_KeyRelease:
          return ET_KEY_RELEASED;
      }
    }
    default:
      break;
  }
  return ET_UNKNOWN;
}

int EventFlagsFromXEvent(const XEvent& xev) {
  switch (xev.type) {
    case KeyPress:
    case KeyRelease: {
      XModifierStateWatcher::GetInstance()->UpdateStateFromXEvent(xev);
      return GetEventFlagsFromXKeyEvent(xev);
    }
    case ButtonPress:
    case ButtonRelease: {
      int flags = GetEventFlagsFromXState(xev.xbutton.state);
      const EventType type = EventTypeFromXEvent(xev);
      if (type == ET_MOUSE_PRESSED || type == ET_MOUSE_RELEASED)
        flags |= GetEventFlagsForButton(xev.xbutton.button);
      return flags;
    }
    case EnterNotify:
      // EnterNotify creates ET_MOUSE_MOVED. Mark as synthesized as this is not
      // a real mouse move event.
      return GetEventFlagsFromXState(xev.xcrossing.state) | EF_IS_SYNTHESIZED;
    case LeaveNotify:
      return GetEventFlagsFromXState(xev.xcrossing.state);
    case MotionNotify:
      return GetEventFlagsFromXState(xev.xmotion.state);
    case GenericEvent: {
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);

      switch (xievent->evtype) {
        case XI_TouchBegin:
        case XI_TouchUpdate:
        case XI_TouchEnd:
          return GetButtonMaskForX2Event(xievent) |
                 GetEventFlagsFromXState(xievent->mods.effective) |
                 GetEventFlagsFromXState(
                     XModifierStateWatcher::GetInstance()->state());
          break;
        case XI_ButtonPress:
        case XI_ButtonRelease: {
          const bool touch =
              TouchFactory::GetInstance()->IsTouchDevice(xievent->sourceid);
          int flags = GetButtonMaskForX2Event(xievent) |
                      GetEventFlagsFromXState(xievent->mods.effective);
          if (touch) {
            flags |= GetEventFlagsFromXState(
                XModifierStateWatcher::GetInstance()->state());
          }

          const EventType type = EventTypeFromXEvent(xev);
          int button = EventButtonFromXEvent(xev);
          if ((type == ET_MOUSE_PRESSED || type == ET_MOUSE_RELEASED) && !touch)
            flags |= GetEventFlagsForButton(button);
          return flags;
        }
        case XI_Motion:
          return GetButtonMaskForX2Event(xievent) |
                 GetEventFlagsFromXState(xievent->mods.effective);
        case XI_KeyPress:
        case XI_KeyRelease: {
          XModifierStateWatcher::GetInstance()->UpdateStateFromXEvent(xev);
          return GetEventFlagsFromXGenericEvent(xev);
        }
      }
    }
  }
  return 0;
}

base::TimeTicks EventTimeFromXEvent(const XEvent& xev) {
  switch (xev.type) {
    case KeyPress:
    case KeyRelease:
      return TimeTicksFromXEventTime(xev.xkey.time);
    case ButtonPress:
    case ButtonRelease:
      return TimeTicksFromXEventTime(xev.xbutton.time);
      break;
    case MotionNotify:
      return TimeTicksFromXEventTime(xev.xmotion.time);
      break;
    case EnterNotify:
    case LeaveNotify:
      return TimeTicksFromXEventTime(xev.xcrossing.time);
      break;
    case GenericEvent: {
      double start, end;
      double touch_timestamp;
      if (GetGestureTimes(xev, &start, &end)) {
        // If the driver supports gesture times, use them.
        return ui::EventTimeStampFromSeconds(end);
      } else if (DeviceDataManagerX11::GetInstance()->GetEventData(
                     xev, DeviceDataManagerX11::DT_TOUCH_RAW_TIMESTAMP,
                     &touch_timestamp)) {
        return ui::EventTimeStampFromSeconds(touch_timestamp);
      } else {
        XIDeviceEvent* xide = static_cast<XIDeviceEvent*>(xev.xcookie.data);
        return TimeTicksFromXEventTime(xide->time);
      }
      break;
    }
  }
  NOTREACHED();
  return base::TimeTicks();
}

gfx::Point EventLocationFromXEvent(const XEvent& xev) {
  switch (xev.type) {
    case EnterNotify:
    case LeaveNotify:
      return gfx::Point(xev.xcrossing.x, xev.xcrossing.y);
    case ButtonPress:
    case ButtonRelease:
      return gfx::Point(xev.xbutton.x, xev.xbutton.y);
    case MotionNotify:
      return gfx::Point(xev.xmotion.x, xev.xmotion.y);
    case GenericEvent: {
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);
      float x = xievent->event_x;
      float y = xievent->event_y;
#if defined(OS_CHROMEOS)
      switch (xievent->evtype) {
        case XI_TouchBegin:
        case XI_TouchUpdate:
        case XI_TouchEnd:
          ui::DeviceDataManagerX11::GetInstance()->ApplyTouchTransformer(
              xievent->deviceid, &x, &y);
          break;
        default:
          break;
      }
#endif  // defined(OS_CHROMEOS)
      return gfx::Point(static_cast<int>(x), static_cast<int>(y));
    }
  }
  return gfx::Point();
}

gfx::Point EventSystemLocationFromXEvent(const XEvent& xev) {
  switch (xev.type) {
    case EnterNotify:
    case LeaveNotify: {
      return gfx::Point(xev.xcrossing.x_root, xev.xcrossing.y_root);
    }
    case ButtonPress:
    case ButtonRelease: {
      return gfx::Point(xev.xbutton.x_root, xev.xbutton.y_root);
    }
    case MotionNotify: {
      return gfx::Point(xev.xmotion.x_root, xev.xmotion.y_root);
    }
    case GenericEvent: {
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);
      return gfx::Point(xievent->root_x, xievent->root_y);
    }
  }

  return gfx::Point();
}

int EventButtonFromXEvent(const XEvent& xev) {
  CHECK_EQ(GenericEvent, xev.type);
  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  int button = xievent->detail;

  return (xievent->sourceid == xievent->deviceid)
             ? DeviceDataManagerX11::GetInstance()->GetMappedButton(button)
             : button;
}

int GetChangedMouseButtonFlagsFromXEvent(const XEvent& xev) {
  switch (xev.type) {
    case ButtonPress:
    case ButtonRelease:
      return GetEventFlagsForButton(xev.xbutton.button);
    case GenericEvent: {
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev.xcookie.data);
      switch (xievent->evtype) {
        case XI_ButtonPress:
        case XI_ButtonRelease:
          return GetEventFlagsForButton(EventButtonFromXEvent(xev));
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
  return 0;
}

gfx::Vector2d GetMouseWheelOffsetFromXEvent(const XEvent& xev) {
  float x_offset, y_offset;
  if (GetScrollOffsetsFromXEvent(xev, &x_offset, &y_offset, NULL, NULL, NULL)) {
    return gfx::Vector2d(static_cast<int>(x_offset),
                         static_cast<int>(y_offset));
  }

  int button = xev.type == GenericEvent ? EventButtonFromXEvent(xev)
                                        : xev.xbutton.button;

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

int GetTouchIdFromXEvent(const XEvent& xev) {
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

float GetTouchRadiusXFromXEvent(const XEvent& xev) {
  double radius = GetTouchParamFromXEvent(
                      xev, ui::DeviceDataManagerX11::DT_TOUCH_MAJOR, 0.0) /
                  2.0;
  ScaleTouchRadius(xev, &radius);
  return radius;
}

float GetTouchRadiusYFromXEvent(const XEvent& xev) {
  double radius = GetTouchParamFromXEvent(
                      xev, ui::DeviceDataManagerX11::DT_TOUCH_MINOR, 0.0) /
                  2.0;
  ScaleTouchRadius(xev, &radius);
  return radius;
}

float GetTouchAngleFromXEvent(const XEvent& xev) {
  return GetTouchParamFromXEvent(
             xev, ui::DeviceDataManagerX11::DT_TOUCH_ORIENTATION, 0.0) /
         2.0;
}

float GetTouchForceFromXEvent(const XEvent& xev) {
  XIDeviceEvent* event = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  if (event->evtype == XI_TouchEnd)
    return 0.0;
  double force = 0.0;
  force = GetTouchParamFromXEvent(
      xev, ui::DeviceDataManagerX11::DT_TOUCH_PRESSURE, 0.0);
  unsigned int deviceid =
      static_cast<XIDeviceEvent*>(xev.xcookie.data)->sourceid;
  // Force is normalized to fall into [0, 1]
  if (!ui::DeviceDataManagerX11::GetInstance()->NormalizeData(
          deviceid, ui::DeviceDataManagerX11::DT_TOUCH_PRESSURE, &force))
    force = 0.0;
  return force;
}

EventPointerType GetTouchPointerTypeFromXEvent(const XEvent& xev) {
  XIDeviceEvent* event = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  DCHECK(ui::TouchFactory::GetInstance()->IsTouchDevice(event->sourceid));
  return ui::TouchFactory::GetInstance()->GetTouchDevicePointerType(
      event->sourceid);
}

PointerDetails GetTouchPointerDetailsFromXEvent(const XEvent& xev) {
  return PointerDetails(
      EventPointerType::POINTER_TYPE_TOUCH, GetTouchIdFromXEvent(xev),
      GetTouchRadiusXFromXEvent(xev), GetTouchRadiusYFromXEvent(xev),
      GetTouchForceFromXEvent(xev), GetTouchAngleFromXEvent(xev));
}

bool GetScrollOffsetsFromXEvent(const XEvent& xev,
                                float* x_offset,
                                float* y_offset,
                                float* x_offset_ordinal,
                                float* y_offset_ordinal,
                                int* finger_count) {
  // Temp values to prevent passing NULLs to DeviceDataManager.
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
    double x_scroll_offset, y_scroll_offset;
    DeviceDataManagerX11::GetInstance()->GetScrollClassOffsets(
        xev, &x_scroll_offset, &y_scroll_offset);
    *x_offset = x_scroll_offset * kWheelScrollAmount;
    *y_offset = y_scroll_offset * kWheelScrollAmount;

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

bool GetFlingDataFromXEvent(const XEvent& xev,
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
  return XModifierStateWatcher::GetInstance()->state() & Mod1Mask;
}

void ResetTimestampRolloverCountersForTesting() {
  g_last_seen_timestamp_ms = 0;
  g_rollover_ms = 0;
}

}  // namespace ui

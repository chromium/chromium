// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/win/events_win_utils.h"

#include <stdint.h>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/win/windowsx_shim.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/events/keycodes/platform_key_map_win.h"
#include "ui/events/types/event_type.h"
#include "ui/events/win/system_event_state_lookup.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

namespace {

// From MSDN: "Mouse" events are flagged with 0xFF515700 if they come
// from a touch or stylus device.  In Vista or later, the eighth bit,
// masked by 0x80, is used to differentiate touch input from pen input
// (0 = pen, 1 = touch).
#define MOUSEEVENTF_FROMTOUCHPEN 0xFF515700
#define MOUSEEVENTF_FROMTOUCH (MOUSEEVENTF_FROMTOUCHPEN | 0x80)
#define SIGNATURE_MASK 0xFFFFFF00

// Get the native mouse key state from the native event message type.
int GetNativeMouseKey(const CHROME_MSG& native_event) {
  switch (native_event.message) {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
      return MK_LBUTTON;
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
      return MK_MBUTTON;
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
      return MK_RBUTTON;
    case WM_NCXBUTTONDBLCLK:
    case WM_NCXBUTTONDOWN:
    case WM_NCXBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
      return MK_XBUTTON1;
  }
  return 0;
}

bool IsButtonDown(const CHROME_MSG& native_event) {
  return ((MK_LBUTTON | MK_MBUTTON | MK_RBUTTON | MK_XBUTTON1 | MK_XBUTTON2) &
          native_event.wParam) != 0;
}

bool IsClientMouseMessage(UINT message) {
  return message == WM_MOUSELEAVE || message == WM_MOUSEHOVER ||
         (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST);
}

bool IsClientMouseEvent(const CHROME_MSG& native_event) {
  return IsClientMouseMessage(native_event.message);
}

bool IsNonClientMouseMessage(UINT message) {
  return message == WM_NCMOUSELEAVE || message == WM_NCMOUSEHOVER ||
         (message >= WM_NCMOUSEMOVE && message <= WM_NCXBUTTONDBLCLK);
}

bool IsNonClientMouseEvent(const CHROME_MSG& native_event) {
  return IsNonClientMouseMessage(native_event.message);
}

bool IsMouseMessage(UINT message) {
  return IsClientMouseMessage(message) || IsNonClientMouseMessage(message);
}

bool IsMouseEvent(const CHROME_MSG& native_event) {
  return IsClientMouseEvent(native_event) ||
         IsNonClientMouseEvent(native_event);
}

bool IsMouseWheelEvent(const CHROME_MSG& native_event) {
  return native_event.message == WM_MOUSEWHEEL ||
         native_event.message == WM_MOUSEHWHEEL;
}

bool IsKeyEvent(const CHROME_MSG& native_event) {
  return native_event.message == WM_KEYDOWN ||
         native_event.message == WM_SYSKEYDOWN ||
         native_event.message == WM_CHAR ||
         native_event.message == WM_SYSCHAR ||
         native_event.message == WM_KEYUP ||
         native_event.message == WM_SYSKEYUP;
}

bool IsScrollEvent(const CHROME_MSG& native_event) {
  return native_event.message == WM_VSCROLL ||
         native_event.message == WM_HSCROLL;
}

// Returns a mask corresponding to the set of pressed modifier keys.
// Checks the current global state and the state sent by client mouse messages.
int KeyStateFlags(const CHROME_MSG& native_event) {
  int flags = GetModifiersFromKeyState();

  // Check key messages for the extended key flag.
  if (IsKeyEvent(native_event) && (HIWORD(native_event.lParam) & KF_EXTENDED))
    flags |= EF_IS_EXTENDED_KEY;

  // Most client mouse messages include key state information.
  if (IsClientMouseEvent(native_event)) {
    int win_flags = GET_KEYSTATE_WPARAM(native_event.wParam);
    flags |= (win_flags & MK_SHIFT) ? EF_SHIFT_DOWN : 0;
    flags |= (win_flags & MK_CONTROL) ? EF_CONTROL_DOWN : 0;
  }

  return flags;
}

// Returns a mask corresponding to the set of pressed mouse buttons.
// This includes the button of the given message, even if it is being released.
int MouseStateFlags(const CHROME_MSG& native_event) {
  int win_flags = GetNativeMouseKey(native_event);

  // Client mouse messages provide key states in their WPARAMs.
  if (IsClientMouseEvent(native_event))
    win_flags |= GET_KEYSTATE_WPARAM(native_event.wParam);

  int flags = 0;
  flags |= (win_flags & MK_LBUTTON) ? EF_LEFT_MOUSE_BUTTON : 0;
  flags |= (win_flags & MK_MBUTTON) ? EF_MIDDLE_MOUSE_BUTTON : 0;
  flags |= (win_flags & MK_RBUTTON) ? EF_RIGHT_MOUSE_BUTTON : 0;
  flags |= IsNonClientMouseEvent(native_event) ? EF_IS_NON_CLIENT : 0;
  return flags;
}

class GetTickCountClock : public base::TickClock {
 public:
  GetTickCountClock() = default;
  ~GetTickCountClock() override = default;

  base::TimeTicks NowTicks() const override {
    return base::TimeTicks() + base::Milliseconds(::GetTickCount());
  }
};

const base::TickClock* g_tick_count_clock = nullptr;

}  // namespace

EventType EventTypeFromMSG(const CHROME_MSG& native_event) {
  switch (native_event.message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_CHAR:
    case WM_SYSCHAR:
      return EventType::kKeyPressed;
    // The WM_DEADCHAR message is posted to the window with the keyboard focus
    // when a WM_KEYUP message is translated. This happens for special keyboard
    // sequences.
    case WM_DEADCHAR:
    case WM_KEYUP:
    // The WM_SYSDEADCHAR message is posted to a window with keyboard focus
    // when the WM_SYSKEYDOWN message is translated by the TranslateMessage
    // function. It specifies the character code of the system dead key.
    case WM_SYSDEADCHAR:
    case WM_SYSKEYUP:
      return EventType::kKeyReleased;
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCLBUTTONDOWN:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCMBUTTONDOWN:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
    case WM_NCXBUTTONDBLCLK:
    case WM_NCXBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
      return EventType::kMousePressed;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_NCLBUTTONUP:
    case WM_NCMBUTTONUP:
    case WM_NCRBUTTONUP:
    case WM_NCXBUTTONUP:
    case WM_RBUTTONUP:
    case WM_XBUTTONUP:
      return EventType::kMouseReleased;
    case WM_MOUSEMOVE:
      return IsButtonDown(native_event) ? EventType::kMouseDragged
                                        : EventType::kMouseMoved;
    case WM_NCMOUSEMOVE:
      return EventType::kMouseMoved;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      return EventType::kMousewheel;
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
      return EventType::kMouseExited;
    case WM_VSCROLL:
    case WM_HSCROLL:
      return EventType::kScroll;
    default:
      // We can't NOTREACHED() here, since this function can be called for any
      // message.
      break;
  }
  return EventType::kUnknown;
}

int EventFlagsFromMSG(const CHROME_MSG& native_event) {
  int flags = KeyStateFlags(native_event);
  if (IsMouseEvent(native_event))
    flags |= MouseStateFlags(native_event);

  return flags;
}

base::TimeTicks EventTimeFromMSG(const CHROME_MSG& native_event) {
  // On Windows, the native input event timestamp (|native_event.time|) is
  // coming from |GetTickCount()| clock [1], while in platform independent code
  // path we get timestamps by calling |TimeTicks::Now()|, which, if using high-
  // resolution timer as underlying implementation, could have different time
  // origin than |GetTickCount()|. To avoid the mismatching, we use
  // |TimeTicks::Now()| for event timestamp instead of the native timestamp to
  // ensure computed input latency and web exposed timestamp is consistent with
  // other components.
  // TODO(crbug.com/342671898): Consider using the new optimistic
  // EventLatencyTimeFromTickClock() conversion here as well.
  // It is unnecessary to invoke |ValidateEventTimeClock| here because of above.
  // [1] http://blogs.msdn.com/b/oldnewthing/archive/2014/01/22/10491576.aspx
  return EventTimeForNow();
}

base::TimeTicks EventLatencyTimeFromTickClock(DWORD event_time,
                                              base::TimeTicks current_time) {
  static const base::NoDestructor<GetTickCountClock> default_tick_count_clock;
  if (!g_tick_count_clock)
    g_tick_count_clock = default_tick_count_clock.get();

  base::TimeTicks event_tick_count =
      base::TimeTicks() + base::Milliseconds(event_time);

  base::TimeTicks current_tick_count = g_tick_count_clock->NowTicks();
  // Check if the 32-bit tick count wrapped around after the event.
  if (current_tick_count < event_tick_count) {
    // ::GetTickCount returns an unsigned 32-bit value, which will fit into the
    // signed 64-bit base::TimeTicks.
    constexpr auto kAdditionalMillisecondsAfterWrap =
        static_cast<int64_t>(std::numeric_limits<DWORD>::max()) + 1;
    current_tick_count += base::Milliseconds(kAdditionalMillisecondsAfterWrap);
  }

  // The Windows ::GetTickCount() clock is simulating an old-school 64hZ clock
  // ticking every 15.625ms and reports that in ms so in practice each "tick" is
  // either 15 or 16ms.
  constexpr auto kMaxTickCountInterval = base::Milliseconds(16);

  base::TimeDelta tick_delta = current_tick_count - event_tick_count;
  // Discount one tick in order to be optimistic about the delay (we cannot know
  // how close the tick clock was to ticking when `event_tick_count` was
  // sampled nor how recently it ticked when `current_tick_count` was sampled).
  tick_delta =
      std::max(base::Microseconds(0), tick_delta - kMaxTickCountInterval);

  base::TimeTicks converted_event_time = current_time - tick_delta;
  ValidateEventTimeClock(&converted_event_time);
  return converted_event_time;
}

base::TimeTicks EventLatencyTimeFromPerformanceCounter(UINT64 event_time) {
  DCHECK(base::TimeTicks::IsHighResolution());
  base::TimeTicks time_stamp = base::TimeTicks::FromQPCValue(event_time);
  ValidateEventTimeClock(&time_stamp);
  return time_stamp;
}

gfx::Point EventLocationFromMSG(const CHROME_MSG& native_event) {
  // This code may use GetCursorPos() to get a mouse location. This may
  // fail in certain situations (see
  // https://bugs.chromium.org/p/chromium/issues/detail?id=540840#c20 for
  // details). To handle failure this code tracks the last known location so
  // that it can use a reasonable value should GetCursorPos() fail.
  static gfx::Point last_known_location;
  gfx::Point event_location = last_known_location;
  if ((native_event.message == WM_MOUSELEAVE ||
       native_event.message == WM_NCMOUSELEAVE) ||
      IsScrollEvent(native_event)) {
    // These events have no coordinates. For sanity with rest of events grab
    // coordinates from the OS.
    POINT native_point;
    if (::GetCursorPos(&native_point)) {
      ScreenToClient(native_event.hwnd, &native_point);
      event_location = gfx::Point(native_point);
    }
  } else if (IsClientMouseEvent(native_event) &&
             !IsMouseWheelEvent(native_event)) {
    // Note: Wheel events are considered client, but their position is in screen
    //       coordinates.
    // Client message. The position is contained in the LPARAM.
    event_location = gfx::Point(static_cast<DWORD>(native_event.lParam));
  } else {
    DCHECK(IsNonClientMouseEvent(native_event) ||
           IsMouseWheelEvent(native_event) || IsScrollEvent(native_event));
    // Non-client message. The position is contained in a POINTS structure in
    // LPARAM, and is in screen coordinates so we have to convert to client.
    POINT native_point;
    native_point.x = GET_X_LPARAM(native_event.lParam);
    native_point.y = GET_Y_LPARAM(native_event.lParam);
    ScreenToClient(native_event.hwnd, &native_point);
    event_location = gfx::Point(native_point);
  }

  last_known_location = event_location;
  return event_location;
}

gfx::Point EventSystemLocationFromMSG(const CHROME_MSG& native_event) {
  POINT global_point = {GET_X_LPARAM(native_event.lParam),
                        GET_Y_LPARAM(native_event.lParam)};
  // Wheel events have position in screen coordinates.
  if (!IsMouseWheelEvent(native_event))
    ClientToScreen(native_event.hwnd, &global_point);
  return gfx::Point(global_point);
}

KeyboardCode KeyboardCodeFromMSG(const CHROME_MSG& native_event) {
  return KeyboardCodeForWindowsKeyCode(static_cast<WORD>(native_event.wParam));
}

DomCode CodeFromMSG(const CHROME_MSG& native_event) {
  const uint16_t scan_code = GetScanCodeFromLParam(native_event.lParam);
  return CodeForWindowsScanCode(scan_code);
}

bool IsCharFromMSG(const CHROME_MSG& native_event) {
  return native_event.message == WM_CHAR || native_event.message == WM_SYSCHAR;
}

int GetChangedMouseButtonFlagsFromMSG(const CHROME_MSG& native_event) {
  switch (GetNativeMouseKey(native_event)) {
    case MK_LBUTTON:
      return EF_LEFT_MOUSE_BUTTON;
    case MK_MBUTTON:
      return EF_MIDDLE_MOUSE_BUTTON;
    case MK_RBUTTON:
      return EF_RIGHT_MOUSE_BUTTON;
    // TODO: add support for MK_XBUTTON1.
    default:
      break;
  }
  return 0;
}

PointerDetails GetMousePointerDetailsFromMSG(const CHROME_MSG& native_event) {
  // We should filter out all the mouse events Synthesized from touch events.
  // TODO(lanwei): Will set the pointer ID, see https://crbug.com/616771.
  if ((GetMessageExtraInfo() & SIGNATURE_MASK) != MOUSEEVENTF_FROMTOUCHPEN)
    return PointerDetails(EventPointerType::kMouse);

  return PointerDetails(EventPointerType::kPen);
}

gfx::Vector2d GetMouseWheelOffsetFromMSG(const CHROME_MSG& native_event) {
  DCHECK(native_event.message == WM_MOUSEWHEEL ||
         native_event.message == WM_MOUSEHWHEEL);
  if (native_event.message == WM_MOUSEWHEEL)
    return gfx::Vector2d(0, GET_WHEEL_DELTA_WPARAM(native_event.wParam));
  return gfx::Vector2d(GET_WHEEL_DELTA_WPARAM(native_event.wParam), 0);
}

void ClearTouchIdIfReleasedFromMSG(const CHROME_MSG& xev) {
  NOTIMPLEMENTED();
}

int GetTouchIdFromMSG(const CHROME_MSG& xev) {
  NOTIMPLEMENTED();
  return 0;
}

PointerDetails GetTouchPointerDetailsFromMSG(const CHROME_MSG& native_event) {
  NOTIMPLEMENTED();
  return PointerDetails(EventPointerType::kTouch,
                        /* pointer_id*/ 0,
                        /* radius_x */ 1.0,
                        /* radius_y */ 1.0,
                        /* force */ 0.f);
}

bool GetScrollOffsetsFromMSG(const CHROME_MSG& native_event) {
  // TODO(ananta)
  // Support retrieving the scroll offsets from the scroll event.
  if (native_event.message == WM_VSCROLL || native_event.message == WM_HSCROLL)
    return true;
  return false;
}

bool GetFlingDataFromMSG(const CHROME_MSG& native_event,
                         float* vx,
                         float* vy,
                         float* vx_ordinal,
                         float* vy_ordinal,
                         bool* is_cancel) {
  // Not supported in Windows.
  NOTIMPLEMENTED();
  return false;
}

int GetModifiersFromKeyState() {
  int modifiers = EF_NONE;
  if (ui::win::IsShiftPressed())
    modifiers |= EF_SHIFT_DOWN;
  if (ui::win::IsAltRightPressed() && PlatformKeyMap::UsesAltGraph()) {
    modifiers |= EF_ALTGR_DOWN;
  } else {
    // Note that if the platform keyboard layout uses AltGraph then these may
    // be overridden on KeyEvents for printable characters generated using
    // AltGraph simulated via Control+Alt.
    if (ui::win::IsCtrlPressed())
      modifiers |= EF_CONTROL_DOWN;
    if (ui::win::IsAltPressed())
      modifiers |= EF_ALT_DOWN;
  }
  if (ui::win::IsWindowsKeyPressed())
    modifiers |= EF_COMMAND_DOWN;
  if (ui::win::IsNumLockOn())
    modifiers |= EF_NUM_LOCK_ON;
  if (ui::win::IsCapsLockOn())
    modifiers |= EF_CAPS_LOCK_ON;
  if (ui::win::IsScrollLockOn())
    modifiers |= EF_SCROLL_LOCK_ON;
  return modifiers;
}

// Windows emulates mouse messages for touch events.
bool IsMouseEventFromTouch(UINT message) {
  return IsMouseMessage(message) &&
         (GetMessageExtraInfo() & MOUSEEVENTF_FROMTOUCH) ==
             MOUSEEVENTF_FROMTOUCH;
}

// Conversion scan_code and LParam each other.
// uint16_t scan_code:
//     ui/events/keycodes/dom/dom_code_data.inc
// 0 - 15bits: represetns the scan code.
// 28 - 30 bits (0xE000): represents whether this is an extended key or not.
//
// LPARAM lParam:
//     http://msdn.microsoft.com/en-us/library/windows/desktop/ms644984.aspx
// 16 - 23bits: represetns the scan code.
// 24bit (0x0100): represents whether this is an extended key or not.
uint16_t GetScanCodeFromLParam(LPARAM l_param) {
  uint16_t scan_code = ((l_param >> 16) & 0x00FF);
  if (l_param & (1 << 24))
    scan_code |= 0xE000;
  return scan_code;
}

LPARAM GetLParamFromScanCode(uint16_t scan_code) {
  LPARAM l_param = static_cast<LPARAM>(scan_code & 0x00FF) << 16;
  if ((scan_code & 0xE000) == 0xE000)
    l_param |= (1 << 24);
  return l_param;
}

KeyEvent KeyEventFromMSG(const CHROME_MSG& msg) {
  DCHECK(IsKeyEvent(msg));
  EventType type = EventTypeFromMSG(msg);
  KeyboardCode key_code = KeyboardCodeFromMSG(msg);
  DomCode code = CodeFromMSG(msg);
  int flags = EventFlagsFromMSG(msg);
  DomKey key;
  base::TimeTicks time_stamp = EventTimeFromMSG(msg);

  if (IsCharFromMSG(msg)) {
    flags = PlatformKeyMap::ReplaceControlAndAltWithAltGraph(flags);
    return KeyEvent::FromCharacter(msg.wParam, key_code, code, flags,
                                   time_stamp);
  } else {
    key = PlatformKeyMap::DomKeyFromKeyboardCode(key_code, &flags);
    return KeyEvent(type, key_code, code, flags, key, time_stamp);
  }
}

CHROME_MSG MSGFromKeyEvent(KeyEvent* event, HWND hwnd) {
  if (event->HasNativeEvent())
    return event->native_event();
  uint16_t scan_code = KeycodeConverter::DomCodeToNativeKeycode(event->code());
  LPARAM l_param = GetLParamFromScanCode(scan_code);
  WPARAM w_param = event->GetConflatedWindowsKeyCode();
  UINT message;
  if (event->is_char())
    message = WM_CHAR;
  else
    message = event->type() == EventType::kKeyPressed ? WM_KEYDOWN : WM_KEYUP;
  return {hwnd, message, w_param, l_param};
}

MouseEvent MouseEventFromMSG(const CHROME_MSG& msg) {
  EventType type = EventTypeFromMSG(msg);
  gfx::Point location = EventLocationFromMSG(msg);
  gfx::Point root_location = EventSystemLocationFromMSG(msg);
  base::TimeTicks time_stamp = EventTimeFromMSG(msg);
  int flags = EventFlagsFromMSG(msg);
  int changed_button_flags = GetChangedMouseButtonFlagsFromMSG(msg);
  PointerDetails pointer_details = GetMousePointerDetailsFromMSG(msg);

  return MouseEvent(type, location, root_location, time_stamp, flags,
                    changed_button_flags, pointer_details);
}

MouseWheelEvent MouseWheelEventFromMSG(const CHROME_MSG& msg) {
  gfx::Vector2d offset = GetMouseWheelOffsetFromMSG(msg);
  gfx::Point location = EventLocationFromMSG(msg);
  gfx::Point root_location = EventSystemLocationFromMSG(msg);
  base::TimeTicks time_stamp = EventTimeFromMSG(msg);
  int flags = EventFlagsFromMSG(msg);
  int changed_button_flags = GetChangedMouseButtonFlagsFromMSG(msg);

  return MouseWheelEvent(offset, location, root_location, time_stamp, flags,
                         changed_button_flags);
}

void SetEventLatencyTickClockForTesting(const base::TickClock* clock) {
  g_tick_count_clock = clock;
}

}  // namespace ui

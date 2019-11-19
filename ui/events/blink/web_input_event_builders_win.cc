// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/web_input_event_builders_win.h"

#include "base/win/windowsx_shim.h"
#include "ui/display/win/screen_win.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event_utils.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace ui {

static const unsigned long kDefaultScrollLinesPerWheelDelta = 3;
static const unsigned long kDefaultScrollCharsPerWheelDelta = 1;

// WebMouseEvent --------------------------------------------------------------

static int g_last_click_count = 0;
static base::TimeTicks g_last_click_time;

static LPARAM GetRelativeCursorPos(HWND hwnd) {
  POINT pos = {-1, -1};
  GetCursorPos(&pos);
  ScreenToClient(hwnd, &pos);
  return MAKELPARAM(pos.x, pos.y);
}

WebMouseEvent WebMouseEventBuilder::Build(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    base::TimeTicks time_stamp,
    blink::WebPointerProperties::PointerType pointer_type) {
  WebInputEvent::Type type = WebInputEvent::Type::kUndefined;
  WebMouseEvent::Button button = WebMouseEvent::Button::kNoButton;
  switch (message) {
    case WM_MOUSEMOVE:
      type = WebInputEvent::kMouseMove;
      if (wparam & MK_LBUTTON)
        button = WebMouseEvent::Button::kLeft;
      else if (wparam & MK_MBUTTON)
        button = WebMouseEvent::Button::kMiddle;
      else if (wparam & MK_RBUTTON)
        button = WebMouseEvent::Button::kRight;
      else
        button = WebMouseEvent::Button::kNoButton;
      break;
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
      // TODO(rbyers): This should be MouseLeave but is disabled temporarily.
      // See http://crbug.com/450631
      type = WebInputEvent::kMouseMove;
      button = WebMouseEvent::Button::kNoButton;
      // set the current mouse position (relative to the client area of the
      // current window) since none is specified for this event
      lparam = GetRelativeCursorPos(hwnd);
      break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
      type = WebInputEvent::kMouseDown;
      button = WebMouseEvent::Button::kLeft;
      break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
      type = WebInputEvent::kMouseDown;
      button = WebMouseEvent::Button::kMiddle;
      break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
      type = WebInputEvent::kMouseDown;
      button = WebMouseEvent::Button::kRight;
      break;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
      type = WebInputEvent::kMouseDown;
      if ((HIWORD(wparam) & XBUTTON1))
        button = WebMouseEvent::Button::kBack;
      else if ((HIWORD(wparam) & XBUTTON2))
        button = WebMouseEvent::Button::kForward;
      break;
    case WM_LBUTTONUP:
      type = WebInputEvent::kMouseUp;
      button = WebMouseEvent::Button::kLeft;
      break;
    case WM_MBUTTONUP:
      type = WebInputEvent::kMouseUp;
      button = WebMouseEvent::Button::kMiddle;
      break;
    case WM_RBUTTONUP:
      type = WebInputEvent::kMouseUp;
      button = WebMouseEvent::Button::kRight;
      break;
    case WM_XBUTTONUP:
      type = WebInputEvent::kMouseUp;
      if ((HIWORD(wparam) & XBUTTON1))
        button = WebMouseEvent::Button::kBack;
      else if ((HIWORD(wparam) & XBUTTON2))
        button = WebMouseEvent::Button::kForward;
      break;
    default:
      NOTREACHED();
  }

  // set modifiers:
  int modifiers =
      ui::EventFlagsToWebEventModifiers(ui::GetModifiersFromKeyState());
  if (wparam & MK_CONTROL)
    modifiers |= WebInputEvent::kControlKey;
  if (wparam & MK_SHIFT)
    modifiers |= WebInputEvent::kShiftKey;
  if (wparam & MK_LBUTTON)
    modifiers |= WebInputEvent::kLeftButtonDown;
  if (wparam & MK_MBUTTON)
    modifiers |= WebInputEvent::kMiddleButtonDown;
  if (wparam & MK_RBUTTON)
    modifiers |= WebInputEvent::kRightButtonDown;
  if (wparam & MK_XBUTTON1)
    modifiers |= WebInputEvent::kBackButtonDown;
  if (wparam & MK_XBUTTON2)
    modifiers |= WebInputEvent::kForwardButtonDown;

  WebMouseEvent result(type, modifiers, time_stamp);
  result.pointer_type = pointer_type;
  result.button = button;

  // set position fields:
  result.SetPositionInWidget(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));

  POINT global_point = {result.PositionInWidget().x,
                        result.PositionInWidget().y};
  ClientToScreen(hwnd, &global_point);

  // We need to convert the global point back to DIP before using it.
  gfx::PointF dip_global_point = display::win::ScreenWin::ScreenToDIPPoint(
      gfx::PointF(global_point.x, global_point.y));

  result.SetPositionInScreen(dip_global_point.x(), dip_global_point.y());

  // calculate number of clicks:

  // This differs slightly from the WebKit code in WebKit/win/WebView.cpp
  // where their original code looks buggy.
  static int last_click_position_x;
  static int last_click_position_y;
  static WebMouseEvent::Button last_click_button = WebMouseEvent::Button::kLeft;

  base::TimeTicks current_time = result.TimeStamp();
  bool cancel_previous_click =
      (abs(last_click_position_x - result.PositionInWidget().x) >
       (::GetSystemMetrics(SM_CXDOUBLECLK) / 2)) ||
      (abs(last_click_position_y - result.PositionInWidget().y) >
       (::GetSystemMetrics(SM_CYDOUBLECLK) / 2)) ||
      ((current_time - g_last_click_time).InMilliseconds() >
       ::GetDoubleClickTime());

  if (result.GetType() == WebInputEvent::kMouseDown) {
    if (!cancel_previous_click && (result.button == last_click_button)) {
      ++g_last_click_count;
    } else {
      g_last_click_count = 1;
      last_click_position_x = result.PositionInWidget().x;
      last_click_position_y = result.PositionInWidget().y;
    }
    g_last_click_time = current_time;
    last_click_button = result.button;
  } else if (result.GetType() == WebInputEvent::kMouseMove ||
             result.GetType() == WebInputEvent::kMouseLeave) {
    if (cancel_previous_click) {
      g_last_click_count = 0;
      last_click_position_x = 0;
      last_click_position_y = 0;
      g_last_click_time = base::TimeTicks();
    }
  }
  result.click_count = g_last_click_count;

  return result;
}

// WebMouseWheelEvent ---------------------------------------------------------

WebMouseWheelEvent WebMouseWheelEventBuilder::Build(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    base::TimeTicks time_stamp,
    blink::WebPointerProperties::PointerType pointer_type) {
  WebMouseWheelEvent result(
      WebInputEvent::kMouseWheel,
      ui::EventFlagsToWebEventModifiers(ui::GetModifiersFromKeyState()),
      time_stamp);

  result.button = WebMouseEvent::Button::kNoButton;
  result.pointer_type = pointer_type;

  // Get key state, coordinates, and wheel delta from event.
  UINT key_state;
  float wheel_delta;
  bool horizontal_scroll = false;
  if ((message == WM_VSCROLL) || (message == WM_HSCROLL)) {
    // Synthesize mousewheel event from a scroll event.  This is needed to
    // simulate middle mouse scrolling in some laptops.  Use GetAsyncKeyState
    // for key state since we are synthesizing the input event.
    key_state = 0;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
      key_state |= MK_SHIFT;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
      key_state |= MK_CONTROL;
    // NOTE: There doesn't seem to be a way to query the mouse button state
    // in this case.

    POINT cursor_position = {0};
    GetCursorPos(&cursor_position);
    result.SetPositionInScreen(cursor_position.x, cursor_position.y);

    switch (LOWORD(wparam)) {
      case SB_LINEUP:  // == SB_LINELEFT
        wheel_delta = WHEEL_DELTA;
        break;
      case SB_LINEDOWN:  // == SB_LINERIGHT
        wheel_delta = -WHEEL_DELTA;
        break;
      case SB_PAGEUP:
        wheel_delta = 1;
        result.delta_units = ui::input_types::ScrollGranularity::kScrollByPage;
        break;
      case SB_PAGEDOWN:
        wheel_delta = -1;
        result.delta_units = ui::input_types::ScrollGranularity::kScrollByPage;
        break;
      default:  // We don't supoprt SB_THUMBPOSITION or SB_THUMBTRACK here.
        wheel_delta = 0;
        break;
    }

    if (message == WM_HSCROLL)
      horizontal_scroll = true;
  } else {
    // Non-synthesized event; we can just read data off the event.
    key_state = GET_KEYSTATE_WPARAM(wparam);

    result.SetPositionInScreen(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));

    // Currently we leave hasPreciseScrollingDeltas false, even for trackpad
    // scrolls that generate WM_MOUSEWHEEL, since we don't have a good way to
    // distinguish these from real mouse wheels (crbug.com/545234).
    wheel_delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam));

    if (message == WM_MOUSEHWHEEL) {
      horizontal_scroll = true;
      wheel_delta = -wheel_delta;  // Windows is <- -/+ ->, WebKit <- +/- ->.
    }
  }

  // Set modifiers based on key state.
  int modifiers = result.GetModifiers();
  if (key_state & MK_SHIFT)
    modifiers |= WebInputEvent::kShiftKey;
  if (key_state & MK_CONTROL)
    modifiers |= WebInputEvent::kControlKey;
  if (key_state & MK_LBUTTON)
    modifiers |= WebInputEvent::kLeftButtonDown;
  if (key_state & MK_MBUTTON)
    modifiers |= WebInputEvent::kMiddleButtonDown;
  if (key_state & MK_RBUTTON)
    modifiers |= WebInputEvent::kRightButtonDown;
  result.SetModifiers(modifiers);

  // Set coordinates by translating event coordinates from screen to client.
  POINT client_point = {result.PositionInScreen().x,
                        result.PositionInScreen().y};
  MapWindowPoints(0, hwnd, &client_point, 1);
  result.SetPositionInWidget(client_point.x, client_point.y);

  // Convert wheel delta amount to a number of pixels to scroll.
  //
  // How many pixels should we scroll per line?  Gecko uses the height of the
  // current line, which means scroll distance changes as you go through the
  // page or go to different pages.  IE 8 is ~60 px/line, although the value
  // seems to vary slightly by page and zoom level.  Also, IE defaults to
  // smooth scrolling while Firefox doesn't, so it can get away with somewhat
  // larger scroll values without feeling as jerky.  Here we use 100 px per
  // three lines (the default scroll amount is three lines per wheel tick).
  // Even though we have smooth scrolling, we don't make this as large as IE
  // because subjectively IE feels like it scrolls farther than you want while
  // reading articles.
  static const float kScrollbarPixelsPerLine = 100.0f / 3.0f;
  wheel_delta /= WHEEL_DELTA;
  float scroll_delta = wheel_delta;
  if (horizontal_scroll) {
    unsigned long scroll_chars = kDefaultScrollCharsPerWheelDelta;
    SystemParametersInfo(SPI_GETWHEELSCROLLCHARS, 0, &scroll_chars, 0);
    // TODO(pkasting): Should probably have a different multiplier
    // scrollbarPixelsPerChar here.
    scroll_delta *= static_cast<float>(scroll_chars) * kScrollbarPixelsPerLine;
  } else {
    unsigned long scroll_lines = kDefaultScrollLinesPerWheelDelta;
    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scroll_lines, 0);
    if (scroll_lines == WHEEL_PAGESCROLL) {
      result.delta_units = ui::input_types::ScrollGranularity::kScrollByPage;
    }

    if (result.delta_units !=
        ui::input_types::ScrollGranularity::kScrollByPage) {
      scroll_delta *=
          static_cast<float>(scroll_lines) * kScrollbarPixelsPerLine;
    }
  }

  // Set scroll amount based on above calculations.  WebKit expects positive
  // deltaY to mean "scroll up" and positive deltaX to mean "scroll left".
  if (horizontal_scroll) {
    result.delta_x = scroll_delta;
    result.wheel_ticks_x = wheel_delta;
  } else {
    result.delta_y = scroll_delta;
    result.wheel_ticks_y = wheel_delta;
  }

  return result;
}

}  // namespace ui

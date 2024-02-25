// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/test/x11_ui_controls_test_helper.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/test/x11_event_waiter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/keysyms/keysyms.h"
#include "ui/gfx/x/xproto.h"

namespace ui {
namespace {

using ui_controls::DOWN;
using ui_controls::LEFT;
using ui_controls::MIDDLE;
using ui_controls::MouseButton;
using ui_controls::RIGHT;
using ui_controls::UP;

// Mask of the buttons currently down.
unsigned button_down_mask = 0;

// The root and time fields of |xevent| may be modified.
template <typename T>
void PostEventToWindowTreeHost(gfx::AcceleratedWidget widget, T* xevent) {
  x11::Window xwindow = static_cast<x11::Window>(widget);
  xevent->event = xwindow;

  xevent->root = x11::Connection::Get()->default_root();
  xevent->time = x11::Time::CurrentTime;

  x11::Connection::Get()->SendEvent(*xevent, xwindow, x11::EventMask::NoEvent);
  x11::Connection::Get()->Flush();
}

}  // namespace

X11UIControlsTestHelper::X11UIControlsTestHelper()
    : connection_(*x11::Connection::Get()),
      x_root_window_(ui::GetX11RootWindow()),
      x_window_(connection_->CreateDummyWindow(
          "Chromium X11UIControlsTestHelper Window")) {}

X11UIControlsTestHelper::~X11UIControlsTestHelper() {
  connection_->DestroyWindow({x_window_});
}

unsigned X11UIControlsTestHelper::ButtonDownMask() const {
  return button_down_mask;
}

void X11UIControlsTestHelper::SendKeyEvents(gfx::AcceleratedWidget widget,
                                            ui::KeyboardCode key,
                                            int key_event_types,
                                            int accelerator_state,
                                            base::OnceClosure closure) {
  bool press = key_event_types & ui_controls::kKeyPress;
  bool release = key_event_types & ui_controls::kKeyRelease;

  bool control = accelerator_state & ui_controls::kControl;
  bool shift = accelerator_state & ui_controls::kShift;
  bool alt = accelerator_state & ui_controls::kAlt;

  x11::KeyEvent xevent;

  // Send key press events.
  if (press) {
    xevent.opcode = x11::KeyEvent::Press;
    if (control) {
      SetKeycodeAndSendThenMask(widget, &xevent, XK_Control_L,
                                x11::KeyButMask::Control);
    }
    if (shift) {
      SetKeycodeAndSendThenMask(widget, &xevent, XK_Shift_L,
                                x11::KeyButMask::Shift);
    }
    if (alt) {
      SetKeycodeAndSendThenMask(widget, &xevent, XK_Alt_L,
                                x11::KeyButMask::Mod1);
    }
    xevent.detail = x11::Connection::Get()->KeysymToKeycode(
        ui::XKeysymForWindowsKeyCode(key, shift));
    PostEventToWindowTreeHost(widget, &xevent);
  }

  // Send key release events.
  if (release) {
    xevent.opcode = x11::KeyEvent::Release;
    PostEventToWindowTreeHost(widget, &xevent);
    if (alt) {
      UnmaskAndSetKeycodeThenSend(widget, &xevent, x11::KeyButMask::Mod1,
                                  XK_Alt_L);
    }
    if (shift) {
      UnmaskAndSetKeycodeThenSend(widget, &xevent, x11::KeyButMask::Shift,
                                  XK_Shift_L);
    }
    if (control) {
      UnmaskAndSetKeycodeThenSend(widget, &xevent, x11::KeyButMask::Control,
                                  XK_Control_L);
    }
  }

  RunClosureAfterAllPendingUIEvents(std::move(closure));
  return;
}

void X11UIControlsTestHelper::SendMouseMotionNotifyEvent(
    gfx::AcceleratedWidget widget,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_loc_in_connection_px,
    base::OnceClosure closure) {
  x11::MotionNotifyEvent xevent{
      .root_x = static_cast<int16_t>(mouse_loc_in_connection_px.x()),
      .root_y = static_cast<int16_t>(mouse_loc_in_connection_px.y()),
      .event_x = static_cast<int16_t>(mouse_loc.x()),
      .event_y = static_cast<int16_t>(mouse_loc.y()),
      .state = static_cast<x11::KeyButMask>(button_down_mask),
      .same_screen = true,
  };
  // RootWindow will take care of other necessary fields.
  PostEventToWindowTreeHost(widget, &xevent);
  RunClosureAfterAllPendingUIEvents(std::move(closure));
  return;
}

void X11UIControlsTestHelper::SendMouseEvent(
    gfx::AcceleratedWidget widget,
    MouseButton type,
    int button_state,
    int accelerator_state,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_loc_in_connection_px,
    base::OnceClosure closure) {
  x11::ButtonEvent xevent;
  xevent.event_x = mouse_loc.x();
  xevent.event_y = mouse_loc.y();
  xevent.root_x = mouse_loc_in_connection_px.x();
  xevent.root_y = mouse_loc_in_connection_px.y();
  switch (type) {
    case LEFT:
      xevent.detail = static_cast<x11::Button>(1);
      xevent.state = x11::KeyButMask::Button1;
      break;
    case MIDDLE:
      xevent.detail = static_cast<x11::Button>(2);
      xevent.state = x11::KeyButMask::Button2;
      break;
    case RIGHT:
      xevent.detail = static_cast<x11::Button>(3);
      xevent.state = x11::KeyButMask::Button3;
      break;
  }

  // Process accelerator key state.
  if (accelerator_state & ui_controls::kShift)
    xevent.state = xevent.state | x11::KeyButMask::Shift;
  if (accelerator_state & ui_controls::kControl)
    xevent.state = xevent.state | x11::KeyButMask::Control;
  if (accelerator_state & ui_controls::kAlt)
    xevent.state = xevent.state | x11::KeyButMask::Mod1;
  if (accelerator_state & ui_controls::kCommand)
    xevent.state = xevent.state | x11::KeyButMask::Mod4;

  // RootWindow will take care of other necessary fields.
  if (button_state & DOWN) {
    xevent.opcode = x11::ButtonEvent::Press;
    PostEventToWindowTreeHost(widget, &xevent);
    button_down_mask |= static_cast<int>(xevent.state);
  }
  if (button_state & UP) {
    xevent.opcode = x11::ButtonEvent::Release;
    PostEventToWindowTreeHost(widget, &xevent);
    int state = static_cast<int>(xevent.state);
    button_down_mask = (button_down_mask | state) ^ state;
  }

  RunClosureAfterAllPendingUIEvents(std::move(closure));
  return;
}

void X11UIControlsTestHelper::RunClosureAfterAllPendingUIEvents(
    base::OnceClosure closure) {
  if (closure.is_null())
    return;
  ui::XEventWaiter::Create(x_window_, std::move(closure));
}

void X11UIControlsTestHelper::SetKeycodeAndSendThenMask(
    gfx::AcceleratedWidget widget,
    x11::KeyEvent* xevent,
    uint32_t keysym,
    x11::KeyButMask mask) {
  xevent->detail = x11::Connection::Get()->KeysymToKeycode(keysym);
  PostEventToWindowTreeHost(widget, xevent);
  xevent->state = xevent->state | mask;
}

void X11UIControlsTestHelper::UnmaskAndSetKeycodeThenSend(
    gfx::AcceleratedWidget widget,
    x11::KeyEvent* xevent,
    x11::KeyButMask mask,
    uint32_t keysym) {
  xevent->state = static_cast<x11::KeyButMask>(
      static_cast<uint32_t>(xevent->state) ^ static_cast<uint32_t>(mask));
  xevent->detail = x11::Connection::Get()->KeysymToKeycode(keysym);
  PostEventToWindowTreeHost(widget, xevent);
}

}  // namespace ui

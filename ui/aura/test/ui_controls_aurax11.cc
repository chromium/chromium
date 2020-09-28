// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/ui_controls_aurax11.h"

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/keysyms/keysyms.h"
#include "ui/gfx/x/xproto.h"

namespace aura {
namespace test {
namespace {

using ui_controls::DOWN;
using ui_controls::LEFT;
using ui_controls::MIDDLE;
using ui_controls::MouseButton;
using ui_controls::RIGHT;
using ui_controls::UIControlsAura;
using ui_controls::UP;

// Mask of the buttons currently down.
unsigned button_down_mask = 0;

}  // namespace

UIControlsX11::UIControlsX11(WindowTreeHost* host) : host_(host) {}

UIControlsX11::~UIControlsX11() = default;

bool UIControlsX11::SendKeyPress(gfx::NativeWindow window,
                                 ui::KeyboardCode key,
                                 bool control,
                                 bool shift,
                                 bool alt,
                                 bool command) {
  return SendKeyPressNotifyWhenDone(window, key, control, shift, alt, command,
                                    base::OnceClosure());
}

bool UIControlsX11::SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                               ui::KeyboardCode key,
                                               bool control,
                                               bool shift,
                                               bool alt,
                                               bool command,
                                               base::OnceClosure closure) {
  x11::KeyEvent xevent;
  xevent.detail = {};
  xevent.opcode = x11::KeyEvent::Press;
  if (control)
    SetKeycodeAndSendThenMask(&xevent, XK_Control_L, x11::KeyButMask::Control);
  if (shift)
    SetKeycodeAndSendThenMask(&xevent, XK_Shift_L, x11::KeyButMask::Shift);
  if (alt)
    SetKeycodeAndSendThenMask(&xevent, XK_Alt_L, x11::KeyButMask::Mod1);
  if (command)
    SetKeycodeAndSendThenMask(&xevent, XK_Meta_L, x11::KeyButMask::Mod4);
  xevent.detail = x11::Connection::Get()->KeysymToKeycode(
      static_cast<x11::KeySym>(ui::XKeysymForWindowsKeyCode(key, shift)));
  PostEventToWindowTreeHost(host_, &xevent);

  // Send key release events.
  xevent.opcode = x11::KeyEvent::Release;
  PostEventToWindowTreeHost(host_, &xevent);
  if (alt)
    UnmaskAndSetKeycodeThenSend(&xevent, x11::KeyButMask::Mod1, XK_Alt_L);
  if (shift)
    UnmaskAndSetKeycodeThenSend(&xevent, x11::KeyButMask::Shift, XK_Shift_L);
  if (control)
    UnmaskAndSetKeycodeThenSend(&xevent, x11::KeyButMask::Control,
                                XK_Control_L);
  if (command)
    UnmaskAndSetKeycodeThenSend(&xevent, x11::KeyButMask::Mod4, XK_Meta_L);
  DCHECK_EQ(xevent.state, x11::KeyButMask{});
  RunClosureAfterAllPendingUIEvents(std::move(closure));
  return true;
}

bool UIControlsX11::SendMouseMove(int screen_x, int screen_y) {
  return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::OnceClosure());
}

bool UIControlsX11::SendMouseMoveNotifyWhenDone(int screen_x,
                                                int screen_y,
                                                base::OnceClosure closure) {
  gfx::Point root_location(screen_x, screen_y);
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(host_->window());
  if (screen_position_client) {
    screen_position_client->ConvertPointFromScreen(host_->window(),
                                                   &root_location);
  }
  gfx::Point root_current_location =
      QueryLatestMousePositionRequestInHost(host_);
  host_->ConvertPixelsToDIP(&root_current_location);

  if (root_location != root_current_location && button_down_mask == 0) {
    // Move the cursor because EnterNotify/LeaveNotify are generated with the
    // current mouse position as a result of XGrabPointer()
    host_->window()->MoveCursorTo(root_location);
  } else {
    x11::MotionNotifyEvent xevent{
        .event_x = root_location.x(),
        .event_y = root_location.y(),
        .state = static_cast<x11::KeyButMask>(button_down_mask),
        .same_screen = true,
    };
    // WindowTreeHost will take care of other necessary fields.
    PostEventToWindowTreeHost(host_, &xevent);
  }
  RunClosureAfterAllPendingUIEvents(std::move(closure));
  return true;
}

bool UIControlsX11::SendMouseEvents(MouseButton type,
                                    int button_state,
                                    int accelerator_state) {
  return SendMouseEventsNotifyWhenDone(type, button_state, base::OnceClosure(),
                                       accelerator_state);
}

bool UIControlsX11::SendMouseEventsNotifyWhenDone(MouseButton type,
                                                  int button_state,
                                                  base::OnceClosure closure,
                                                  int accelerator_state) {
  x11::ButtonEvent xevent;
  gfx::Point mouse_loc = Env::GetInstance()->last_mouse_location();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(host_->window());
  if (screen_position_client) {
    screen_position_client->ConvertPointFromScreen(host_->window(), &mouse_loc);
  }
  xevent.event_x = mouse_loc.x();
  xevent.event_y = mouse_loc.y();
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

  // WindowEventDispatcher will take care of other necessary fields.
  if (button_state & DOWN) {
    xevent.opcode = x11::ButtonEvent::Press;
    PostEventToWindowTreeHost(host_, &xevent);
    button_down_mask |= static_cast<int>(xevent.state);
  }
  if (button_state & UP) {
    xevent.opcode = x11::ButtonEvent::Release;
    PostEventToWindowTreeHost(host_, &xevent);
    int state = static_cast<int>(xevent.state);
    button_down_mask = (button_down_mask | state) ^ state;
  }
  RunClosureAfterAllPendingUIEvents(std::move(closure));
  return true;
}

bool UIControlsX11::SendMouseClick(MouseButton type) {
  return SendMouseEvents(type, UP | DOWN, ui_controls::kNoAccelerator);
}

void UIControlsX11::RunClosureAfterAllPendingUIEvents(
    base::OnceClosure closure) {
  if (closure.is_null())
    return;
  ui::XEventWaiter::Create(
      static_cast<x11::Window>(host_->GetAcceleratedWidget()),
      std::move(closure));
}

void UIControlsX11::SetKeycodeAndSendThenMask(x11::KeyEvent* xevent,
                                              KeySym keysym,
                                              x11::KeyButMask mask) {
  xevent->detail =
      x11::Connection::Get()->KeysymToKeycode(static_cast<x11::KeySym>(keysym));
  PostEventToWindowTreeHost(host_, xevent);
  xevent->state = xevent->state | mask;
}

void UIControlsX11::UnmaskAndSetKeycodeThenSend(x11::KeyEvent* xevent,
                                                x11::KeyButMask mask,
                                                KeySym keysym) {
  xevent->state = static_cast<x11::KeyButMask>(
      static_cast<uint32_t>(xevent->state) ^ static_cast<uint32_t>(mask));
  xevent->detail =
      x11::Connection::Get()->KeysymToKeycode(static_cast<x11::KeySym>(keysym));
  PostEventToWindowTreeHost(host_, xevent);
}

}  // namespace test
}  // namespace aura

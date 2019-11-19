// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/test/x11_event_sender.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/compositor/dip_util.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/test/platform_event_waiter.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"

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

// Returns atom that indidates that the XEvent is marker event.
Atom MarkerEventAtom() {
  return gfx::GetAtom("marker_event");
}

// Returns true when the event is a marker event.
bool Matcher(const ui::PlatformEvent& event) {
  return event->xany.type == ClientMessage &&
      event->xclient.message_type == MarkerEventAtom();
}

class UIControlsX11 : public UIControlsAura {
 public:
  UIControlsX11(WindowTreeHost* host) : host_(host) {
  }

  bool SendKeyPress(gfx::NativeWindow window,
                    ui::KeyboardCode key,
                    bool control,
                    bool shift,
                    bool alt,
                    bool command) override {
    return SendKeyPressNotifyWhenDone(window, key, control, shift, alt, command,
                                      base::OnceClosure());
  }
  bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                  ui::KeyboardCode key,
                                  bool control,
                                  bool shift,
                                  bool alt,
                                  bool command,
                                  base::OnceClosure closure) override {
    XEvent xevent;
    xevent.xkey = {};
    xevent.xkey.type = KeyPress;
    if (control)
      SetKeycodeAndSendThenMask(&xevent, XK_Control_L, ControlMask);
    if (shift)
      SetKeycodeAndSendThenMask(&xevent, XK_Shift_L, ShiftMask);
    if (alt)
      SetKeycodeAndSendThenMask(&xevent, XK_Alt_L, Mod1Mask);
    if (command)
      SetKeycodeAndSendThenMask(&xevent, XK_Meta_L, Mod4Mask);
    xevent.xkey.keycode =
        XKeysymToKeycode(gfx::GetXDisplay(),
                         ui::XKeysymForWindowsKeyCode(key, shift));
    PostEventToWindowTreeHost(xevent, host_);

    // Send key release events.
    xevent.xkey.type = KeyRelease;
    PostEventToWindowTreeHost(xevent, host_);
    if (alt)
      UnmaskAndSetKeycodeThenSend(&xevent, Mod1Mask, XK_Alt_L);
    if (shift)
      UnmaskAndSetKeycodeThenSend(&xevent, ShiftMask, XK_Shift_L);
    if (control)
      UnmaskAndSetKeycodeThenSend(&xevent, ControlMask, XK_Control_L);
    if (command)
      UnmaskAndSetKeycodeThenSend(&xevent, Mod4Mask, XK_Meta_L);
    DCHECK(!xevent.xkey.state);
    RunClosureAfterAllPendingUIEvents(std::move(closure));
    return true;
  }

  bool SendMouseMove(long screen_x, long screen_y) override {
    return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::OnceClosure());
  }
  bool SendMouseMoveNotifyWhenDone(long screen_x,
                                   long screen_y,
                                   base::OnceClosure closure) override {
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
      XEvent xevent;
      xevent.xmotion = {};
      XMotionEvent* xmotion = &xevent.xmotion;
      xmotion->type = MotionNotify;
      xmotion->x = root_location.x();
      xmotion->y = root_location.y();
      xmotion->state = button_down_mask;
      xmotion->same_screen = x11::True;
      // WindowTreeHost will take care of other necessary fields.
      PostEventToWindowTreeHost(xevent, host_);
    }
    RunClosureAfterAllPendingUIEvents(std::move(closure));
    return true;
  }
  bool SendMouseEvents(MouseButton type,
                       int button_state,
                       int accelerator_state) override {
    return SendMouseEventsNotifyWhenDone(
        type, button_state, base::OnceClosure(), accelerator_state);
  }
  bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                     int button_state,
                                     base::OnceClosure closure,
                                     int accelerator_state) override {
    XEvent xevent;
    xevent.xbutton = {};
    XButtonEvent* xbutton = &xevent.xbutton;
    gfx::Point mouse_loc = Env::GetInstance()->last_mouse_location();
    aura::client::ScreenPositionClient* screen_position_client =
          aura::client::GetScreenPositionClient(host_->window());
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(host_->window(),
                                                     &mouse_loc);
    }
    xbutton->x = mouse_loc.x();
    xbutton->y = mouse_loc.y();
    xbutton->same_screen = x11::True;
    switch (type) {
      case LEFT:
        xbutton->button = Button1;
        xbutton->state = Button1Mask;
        break;
      case MIDDLE:
        xbutton->button = Button2;
        xbutton->state = Button2Mask;
        break;
      case RIGHT:
        xbutton->button = Button3;
        xbutton->state = Button3Mask;
        break;
    }

    // Process accelerator key state.
    if (accelerator_state & ui_controls::kShift)
      xbutton->state |= ShiftMask;
    if (accelerator_state & ui_controls::kControl)
      xbutton->state |= ControlMask;
    if (accelerator_state & ui_controls::kAlt)
      xbutton->state |= Mod1Mask;
    if (accelerator_state & ui_controls::kCommand)
      xbutton->state |= Mod4Mask;

    // WindowEventDispatcher will take care of other necessary fields.
    if (button_state & DOWN) {
      xevent.xbutton.type = ButtonPress;
      PostEventToWindowTreeHost(xevent, host_);
      button_down_mask |= xbutton->state;
    }
    if (button_state & UP) {
      xevent.xbutton.type = ButtonRelease;
      PostEventToWindowTreeHost(xevent, host_);
      button_down_mask = (button_down_mask | xbutton->state) ^ xbutton->state;
    }
    RunClosureAfterAllPendingUIEvents(std::move(closure));
    return true;
  }
  bool SendMouseClick(MouseButton type) override {
    return SendMouseEvents(type, UP | DOWN, ui_controls::kNoAccelerator);
  }
  void RunClosureAfterAllPendingUIEvents(base::OnceClosure closure) {
    if (closure.is_null())
      return;
    static XEvent* marker_event = NULL;
    if (!marker_event) {
      marker_event = new XEvent();
      marker_event->xclient.type = ClientMessage;
      marker_event->xclient.display = NULL;
      marker_event->xclient.window = x11::None;
      marker_event->xclient.format = 8;
    }
    marker_event->xclient.message_type = MarkerEventAtom();
    PostEventToWindowTreeHost(*marker_event, host_);
    ui::PlatformEventWaiter::Create(std::move(closure),
                                    base::BindRepeating(&Matcher));
  }
 private:
  void SetKeycodeAndSendThenMask(XEvent* xevent,
                                 KeySym keysym,
                                 unsigned int mask) {
    xevent->xkey.keycode =
        XKeysymToKeycode(gfx::GetXDisplay(), keysym);
    PostEventToWindowTreeHost(*xevent, host_);
    xevent->xkey.state |= mask;
  }

  void UnmaskAndSetKeycodeThenSend(XEvent* xevent,
                                   unsigned int mask,
                                   KeySym keysym) {
    xevent->xkey.state ^= mask;
    xevent->xkey.keycode =
        XKeysymToKeycode(gfx::GetXDisplay(), keysym);
    PostEventToWindowTreeHost(*xevent, host_);
  }

  WindowTreeHost* host_;

  DISALLOW_COPY_AND_ASSIGN(UIControlsX11);
};

}  // namespace

UIControlsAura* CreateUIControlsAura(WindowTreeHost* host) {
  return new UIControlsX11(host);
}

}  // namespace test
}  // namespace aura

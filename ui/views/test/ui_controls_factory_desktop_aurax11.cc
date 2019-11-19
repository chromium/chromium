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
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/base/x/x11_util.h"
#include "ui/compositor/dip_util.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/test/platform_event_waiter.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_connection.h"
#include "ui/views/test/test_desktop_screen_x11.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

namespace views {
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

class UIControlsDesktopX11 : public UIControlsAura {
 public:
  UIControlsDesktopX11()
      : x_display_(gfx::GetXDisplay()),
        x_root_window_(DefaultRootWindow(x_display_)),
        x_window_(XCreateWindow(x_display_,
                                x_root_window_,
                                -100,            // x
                                -100,            // y
                                10,              // width
                                10,              // height
                                0,               // border width
                                CopyFromParent,  // depth
                                InputOnly,
                                CopyFromParent,  // visual
                                0,
                                nullptr)) {
    XStoreName(x_display_, x_window_, "Chromium UIControlsDesktopX11 Window");
  }

  ~UIControlsDesktopX11() override { XDestroyWindow(x_display_, x_window_); }

  bool SendKeyPress(gfx::NativeWindow window,
                    ui::KeyboardCode key,
                    bool control,
                    bool shift,
                    bool alt,
                    bool command) override {
    DCHECK(!command);  // No command key on Aura
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
    DCHECK(!command);  // No command key on Aura

    aura::WindowTreeHost* host = window->GetHost();

    XEvent xevent;
    xevent.xkey = {};
    xevent.xkey.type = KeyPress;
    if (control) {
      SetKeycodeAndSendThenMask(host, &xevent, XK_Control_L, ControlMask);
    }
    if (shift)
      SetKeycodeAndSendThenMask(host, &xevent, XK_Shift_L, ShiftMask);
    if (alt)
      SetKeycodeAndSendThenMask(host, &xevent, XK_Alt_L, Mod1Mask);
    xevent.xkey.keycode =
        XKeysymToKeycode(x_display_,
                         ui::XKeysymForWindowsKeyCode(key, shift));
    aura::test::PostEventToWindowTreeHost(xevent, host);

    // Send key release events.
    xevent.xkey.type = KeyRelease;
    aura::test::PostEventToWindowTreeHost(xevent, host);
    if (alt)
      UnmaskAndSetKeycodeThenSend(host, &xevent, Mod1Mask, XK_Alt_L);
    if (shift)
      UnmaskAndSetKeycodeThenSend(host, &xevent, ShiftMask, XK_Shift_L);
    if (control) {
      UnmaskAndSetKeycodeThenSend(host, &xevent, ControlMask, XK_Control_L);
    }
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
    gfx::Point screen_location(screen_x, screen_y);
    gfx::Point root_location = screen_location;
    aura::Window* root_window = RootWindowForPoint(screen_location);

    aura::client::ScreenPositionClient* screen_position_client =
          aura::client::GetScreenPositionClient(root_window);
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(root_window,
                                                     &root_location);
    }

    aura::WindowTreeHost* host = root_window->GetHost();
    gfx::Point root_current_location =
        aura::test::QueryLatestMousePositionRequestInHost(host);
    host->ConvertPixelsToDIP(&root_current_location);

    auto* screen = views::test::TestDesktopScreenX11::GetInstance();
    DCHECK_EQ(screen, display::Screen::GetScreen());
    screen->set_cursor_screen_point(gfx::Point(screen_x, screen_y));

    if (root_location != root_current_location && button_down_mask == 0) {
      // Move the cursor because EnterNotify/LeaveNotify are generated with the
      // current mouse position as a result of XGrabPointer()
      root_window->MoveCursorTo(root_location);
    } else {
      XEvent xevent;
      xevent.xmotion = {};
      XMotionEvent* xmotion = &xevent.xmotion;
      xmotion->type = MotionNotify;
      xmotion->x = root_location.x();
      xmotion->y = root_location.y();
      xmotion->state = button_down_mask;
      xmotion->same_screen = x11::True;
      // RootWindow will take care of other necessary fields.
      aura::test::PostEventToWindowTreeHost(xevent, host);
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
    gfx::Point mouse_loc = aura::Env::GetInstance()->last_mouse_location();
    aura::Window* root_window = RootWindowForPoint(mouse_loc);
    aura::client::ScreenPositionClient* screen_position_client =
          aura::client::GetScreenPositionClient(root_window);
    if (screen_position_client)
      screen_position_client->ConvertPointFromScreen(root_window, &mouse_loc);
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
    // Process the accelerator key state.
    if (accelerator_state & ui_controls::kShift)
      xbutton->state |= ShiftMask;
    if (accelerator_state & ui_controls::kControl)
      xbutton->state |= ControlMask;
    if (accelerator_state & ui_controls::kAlt)
      xbutton->state |= Mod1Mask;
    if (accelerator_state & ui_controls::kCommand)
      xbutton->state |= Mod4Mask;

    // RootWindow will take care of other necessary fields.
    if (button_state & DOWN) {
      xevent.xbutton.type = ButtonPress;
      aura::test::PostEventToWindowTreeHost(xevent, root_window->GetHost());
      button_down_mask |= xbutton->state;
    }
    if (button_state & UP) {
      xevent.xbutton.type = ButtonRelease;
      aura::test::PostEventToWindowTreeHost(xevent, root_window->GetHost());
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
    static XEvent* marker_event = nullptr;
    if (!marker_event) {
      marker_event = new XEvent();
      marker_event->xclient.type = ClientMessage;
      marker_event->xclient.display = x_display_;
      marker_event->xclient.window = x_window_;
      marker_event->xclient.format = 8;
    }
    marker_event->xclient.message_type = MarkerEventAtom();
    XSendEvent(x_display_, x_window_, x11::False, 0, marker_event);
    ui::PlatformEventWaiter::Create(std::move(closure),
                                    base::BindRepeating(&Matcher));
  }
 private:
  aura::Window* RootWindowForPoint(const gfx::Point& point) {
    // Most interactive_ui_tests run inside of the aura_test_helper
    // environment. This means that we can't rely on display::Screen and several
    // other things to work properly. Therefore we hack around this by
    // iterating across the windows owned DesktopWindowTreeHostLinux since this
    // doesn't rely on having a DesktopScreenX11.
    std::vector<aura::Window*> windows =
        DesktopWindowTreeHostLinux::GetAllOpenWindows();
    const auto i =
        std::find_if(windows.cbegin(), windows.cend(), [point](auto* window) {
          return window->GetBoundsInScreen().Contains(point) ||
                 window->HasCapture();
        });
    DCHECK(i != windows.cend()) << "Couldn't find RW for " << point.ToString()
                                << " among " << windows.size() << " RWs.";
    return (*i)->GetRootWindow();
  }

  void SetKeycodeAndSendThenMask(aura::WindowTreeHost* host,
                                 XEvent* xevent,
                                 KeySym keysym,
                                 unsigned int mask) {
    xevent->xkey.keycode = XKeysymToKeycode(x_display_, keysym);
    aura::test::PostEventToWindowTreeHost(*xevent, host);
    xevent->xkey.state |= mask;
  }

  void UnmaskAndSetKeycodeThenSend(aura::WindowTreeHost* host,
                                   XEvent* xevent,
                                   unsigned int mask,
                                   KeySym keysym) {
    xevent->xkey.state ^= mask;
    xevent->xkey.keycode = XKeysymToKeycode(x_display_, keysym);
    aura::test::PostEventToWindowTreeHost(*xevent, host);
  }

  // Our X11 state.
  Display* x_display_;
  ::Window x_root_window_;

  // Input-only window used for events.
  ::Window x_window_;

  DISALLOW_COPY_AND_ASSIGN(UIControlsDesktopX11);
};

}  // namespace

UIControlsAura* CreateUIControlsDesktopAura() {
  // The constructor of UIControlsDesktopX11 needs X11 connection to be
  // initialized.
  gfx::InitializeThreadedX11();
  return new UIControlsDesktopX11();
}

}  // namespace test
}  // namespace views

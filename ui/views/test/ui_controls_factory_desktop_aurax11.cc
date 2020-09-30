// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/ui_controls_factory_desktop_aurax11.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/test/x11_event_sender.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/test/x11_event_waiter.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/keysyms/keysyms.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gfx/x/xproto.h"
#include "ui/views/test/test_desktop_screen_x11.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

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

class UIControlsDesktopX11 : public UIControlsAura {
 public:
  UIControlsDesktopX11()
      : connection_(x11::Connection::Get()),
        x_root_window_(ui::GetX11RootWindow()),
        x_window_(
            ui::CreateDummyWindow("Chromium UIControlsDesktopX11 Window")) {}

  ~UIControlsDesktopX11() override { connection_->DestroyWindow({x_window_}); }

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

    x11::KeyEvent xevent;
    xevent.opcode = x11::KeyEvent::Press;
    if (control) {
      SetKeycodeAndSendThenMask(host, &xevent, XK_Control_L,
                                x11::KeyButMask::Control);
    }
    if (shift)
      SetKeycodeAndSendThenMask(host, &xevent, XK_Shift_L,
                                x11::KeyButMask::Shift);
    if (alt)
      SetKeycodeAndSendThenMask(host, &xevent, XK_Alt_L, x11::KeyButMask::Mod1);
    xevent.detail = x11::Connection::Get()->KeysymToKeycode(
        static_cast<x11::KeySym>(ui::XKeysymForWindowsKeyCode(key, shift)));
    aura::test::PostEventToWindowTreeHost(host, &xevent);

    // Send key release events.
    xevent.opcode = x11::KeyEvent::Release;
    aura::test::PostEventToWindowTreeHost(host, &xevent);
    if (alt) {
      UnmaskAndSetKeycodeThenSend(host, &xevent, x11::KeyButMask::Mod1,
                                  XK_Alt_L);
    }
    if (shift) {
      UnmaskAndSetKeycodeThenSend(host, &xevent, x11::KeyButMask::Shift,
                                  XK_Shift_L);
    }
    if (control) {
      UnmaskAndSetKeycodeThenSend(host, &xevent, x11::KeyButMask::Control,
                                  XK_Control_L);
    }
    DCHECK_EQ(xevent.state, x11::KeyButMask{});
    RunClosureAfterAllPendingUIEvents(std::move(closure));
    return true;
  }

  bool SendMouseMove(int screen_x, int screen_y) override {
    return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::OnceClosure());
  }
  bool SendMouseMoveNotifyWhenDone(int screen_x,
                                   int screen_y,
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
      x11::MotionNotifyEvent xevent{
          .event_x = root_location.x(),
          .event_y = root_location.y(),
          .state = static_cast<x11::KeyButMask>(button_down_mask),
          .same_screen = true,
      };
      // RootWindow will take care of other necessary fields.
      aura::test::PostEventToWindowTreeHost(host, &xevent);
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
    x11::ButtonEvent xevent;
    gfx::Point mouse_loc = aura::Env::GetInstance()->last_mouse_location();
    aura::Window* root_window = RootWindowForPoint(mouse_loc);
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root_window);
    if (screen_position_client)
      screen_position_client->ConvertPointFromScreen(root_window, &mouse_loc);
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

    // RootWindow will take care of other necessary fields.
    if (button_state & DOWN) {
      xevent.opcode = x11::ButtonEvent::Press;
      aura::test::PostEventToWindowTreeHost(root_window->GetHost(), &xevent);
      button_down_mask |= static_cast<int>(xevent.state);
    }
    if (button_state & UP) {
      xevent.opcode = x11::ButtonEvent::Release;
      aura::test::PostEventToWindowTreeHost(root_window->GetHost(), &xevent);
      int state = static_cast<int>(xevent.state);
      button_down_mask = (button_down_mask | state) ^ state;
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
    ui::XEventWaiter::Create(x_window_, std::move(closure));
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
                                 x11::KeyEvent* xevent,
                                 KeySym keysym,
                                 x11::KeyButMask mask) {
    xevent->detail = x11::Connection::Get()->KeysymToKeycode(
        static_cast<x11::KeySym>(keysym));
    aura::test::PostEventToWindowTreeHost(host, xevent);
    xevent->state = xevent->state | mask;
  }

  void UnmaskAndSetKeycodeThenSend(aura::WindowTreeHost* host,
                                   x11::KeyEvent* xevent,
                                   x11::KeyButMask mask,
                                   KeySym keysym) {
    xevent->state = static_cast<x11::KeyButMask>(
        static_cast<uint32_t>(xevent->state) ^ static_cast<uint32_t>(mask));
    xevent->detail = x11::Connection::Get()->KeysymToKeycode(
        static_cast<x11::KeySym>(keysym));
    aura::test::PostEventToWindowTreeHost(host, xevent);
  }

  // Our X11 state.
  x11::Connection* connection_;
  x11::Window x_root_window_;

  // Input-only window used for events.
  x11::Window x_window_;

  DISALLOW_COPY_AND_ASSIGN(UIControlsDesktopX11);
};

}  // namespace

UIControlsAura* CreateUIControlsDesktopAura() {
  return new UIControlsDesktopX11();
}

}  // namespace test
}  // namespace views

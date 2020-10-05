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
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/base/x/test/x11_ui_controls_test_helper.h"
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

class UIControlsDesktopX11 : public UIControlsAura {
 public:
  UIControlsDesktopX11() = default;
  ~UIControlsDesktopX11() override = default;

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
    x11_ui_controls_test_helper_.SendKeyPressEvent(host->GetAcceleratedWidget(),
                                                   key, control, shift, alt,
                                                   command, std::move(closure));
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

    if (root_location != root_current_location &&
        x11_ui_controls_test_helper_.ButtonDownMask() == 0) {
      // Move the cursor because EnterNotify/LeaveNotify are generated with the
      // current mouse position as a result of XGrabPointer()
      root_window->MoveCursorTo(root_location);
    } else {
      gfx::Point screen_point(root_location);
      host->ConvertDIPToScreenInPixels(&screen_point);
      x11_ui_controls_test_helper_.SendMouseMotionNotifyEvent(
          host->GetAcceleratedWidget(), root_location, screen_point,
          std::move(closure));
    }
    x11_ui_controls_test_helper_.RunClosureAfterAllPendingUIEvents(
        std::move(closure));
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
    gfx::Point mouse_loc = aura::Env::GetInstance()->last_mouse_location();
    aura::Window* root_window = RootWindowForPoint(mouse_loc);
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root_window);
    if (screen_position_client)
      screen_position_client->ConvertPointFromScreen(root_window, &mouse_loc);

    gfx::Point mouse_root_loc = mouse_loc;
    root_window->GetHost()->ConvertDIPToScreenInPixels(&mouse_root_loc);
    x11_ui_controls_test_helper_.SendMouseEvent(
        root_window->GetHost()->GetAcceleratedWidget(), type, button_state,
        accelerator_state, mouse_loc, mouse_root_loc, std::move(closure));
    return true;
  }
  bool SendMouseClick(MouseButton type) override {
    return SendMouseEvents(type, UP | DOWN, ui_controls::kNoAccelerator);
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

  ui::X11UIControlsTestHelper x11_ui_controls_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(UIControlsDesktopX11);
};

}  // namespace

UIControlsAura* CreateUIControlsDesktopAura() {
  return new UIControlsDesktopX11();
}

}  // namespace test
}  // namespace views

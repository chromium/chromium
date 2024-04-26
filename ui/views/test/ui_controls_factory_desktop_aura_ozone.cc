// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_ui_controls_test_helper.h"
#include "ui/views/test/test_desktop_screen_ozone.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace {

ui::OzoneUIControlsTestHelper* g_ozone_ui_controls_test_helper = nullptr;

using ui_controls::DOWN;
using ui_controls::LEFT;
using ui_controls::MIDDLE;
using ui_controls::MouseButton;
using ui_controls::RIGHT;
using ui_controls::UP;

aura::Window* RootWindowForPoint(const gfx::Point& point,
                                 aura::Window* window_hint = nullptr) {
  // Most interactive_ui_tests run inside of the aura_test_helper
  // environment. This means that we can't rely on display::Screen and several
  // other things to work properly. Therefore we hack around this by
  // iterating across the windows owned DesktopWindowTreeHostLinux since this
  // doesn't rely on having a DesktopScreenX11.
  std::vector<aura::Window*> windows =
      views::DesktopWindowTreeHostPlatform::GetAllOpenWindows();
  const auto i = base::ranges::find_if(windows, [point](auto* window) {
    return window->GetBoundsInScreen().Contains(point) || window->HasCapture();
  });

  // Compare the window we found (if any) and the window hint (again, if any).
  // If there is a hint and a window with capture they had better be the same
  // or the test is trying to do something that can't actually happen.
  aura::Window* const found =
      i != windows.cend() ? (*i)->GetRootWindow() : nullptr;
  aura::Window* const hint =
      window_hint ? window_hint->GetRootWindow() : nullptr;
  if (found && hint && found->HasCapture()) {
    CHECK_EQ(found, hint);
  }
  return hint ? hint : found;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
aura::Window* TopRootWindow() {
  std::vector<aura::Window*> windows =
      views::DesktopWindowTreeHostPlatform::GetAllOpenWindows();
  DCHECK(!windows.empty());
  return windows[0]->GetRootWindow();
}
#endif

}  // namespace

namespace ui_controls {

void EnableUIControls() {
  // TODO(crbug.com/40249511): This gets called twice in some tests.
  // Add DCHECK once these tests are fixed.
  if (!g_ozone_ui_controls_test_helper) {
    g_ozone_ui_controls_test_helper =
        ui::CreateOzoneUIControlsTestHelper().release();
  }
}

void ResetUIControlsIfEnabled() {
  if (g_ozone_ui_controls_test_helper) {
    g_ozone_ui_controls_test_helper->Reset();
  }
}

// An interface to provide Aura implementation of UI control.
// static
bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  DCHECK(!command);  // No command key on Aura
  return SendKeyPressNotifyWhenDone(window, key, control, shift, alt, command,
                                    base::OnceClosure());
}

// static
bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                base::OnceClosure closure,
                                KeyEventType wait_for) {
  // This doesn't time out if `window` is deleted before the key release events
  // are dispatched, so it's fine to ignore `wait_for` and always wait for key
  // release events.
  DCHECK(!command);  // No command key on Aura
  return SendKeyEventsNotifyWhenDone(
      window, key, kKeyPress | kKeyRelease, std::move(closure),
      GenerateAcceleratorState(control, shift, alt, command));
}

// static
bool SendKeyEvents(gfx::NativeWindow window,
                   ui::KeyboardCode key,
                   int key_event_types,
                   int accelerator_state) {
  DCHECK(!(accelerator_state & kCommand));  // No command key on Aura
  return SendKeyEventsNotifyWhenDone(window, key, key_event_types,
                                     base::OnceClosure(), accelerator_state);
}

// static
bool SendKeyEventsNotifyWhenDone(gfx::NativeWindow window,
                                 ui::KeyboardCode key,
                                 int key_event_types,
                                 base::OnceClosure closure,
                                 int accelerator_state) {
  DCHECK(g_ozone_ui_controls_test_helper);
  DCHECK(!(accelerator_state & kCommand));  // No command key on Aura
  aura::WindowTreeHost* host = window->GetHost();
  g_ozone_ui_controls_test_helper->SendKeyEvents(
      host->GetAcceleratedWidget(), key, key_event_types, accelerator_state,
      std::move(closure));
  return true;
}

// static
bool SendMouseMove(int screen_x, int screen_y, gfx::NativeWindow window_hint) {
  return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::OnceClosure(),
                                     window_hint);
}

// static
bool SendMouseMoveNotifyWhenDone(int screen_x,
                                 int screen_y,
                                 base::OnceClosure task,
                                 gfx::NativeWindow window_hint) {
  if (g_ozone_ui_controls_test_helper->SupportsScreenCoordinates()) {
    window_hint = nullptr;
  }

  gfx::Point screen_location(screen_x, screen_y);
  gfx::Point root_location = screen_location;
  aura::Window* root_window = RootWindowForPoint(screen_location, window_hint);
  if (root_window == nullptr) {
    return true;
  }

  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (screen_position_client) {
    screen_position_client->ConvertPointFromScreen(root_window, &root_location);
  }

  aura::WindowTreeHost* host = root_window->GetHost();
  gfx::Point root_current_location =
      aura::test::QueryLatestMousePositionRequestInHost(host);
  host->ConvertPixelsToDIP(&root_current_location);

  auto* screen = views::test::TestDesktopScreenOzone::GetInstance();
  DCHECK_EQ(screen, display::Screen::GetScreen());
  screen->set_cursor_screen_point(gfx::Point(screen_x, screen_y));

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (root_location != root_current_location &&
      !g_ozone_ui_controls_test_helper->MustUseUiControlsForMoveCursorTo() &&
      g_ozone_ui_controls_test_helper->ButtonDownMask() == 0) {
    // Move the cursor because EnterNotify/LeaveNotify are generated with the
    // current mouse position as a result of XGrabPointer()
    root_window->MoveCursorTo(root_location);
    g_ozone_ui_controls_test_helper->RunClosureAfterAllPendingUIEvents(
        std::move(task));
    return true;
  }
#endif

  g_ozone_ui_controls_test_helper->SendMouseMotionNotifyEvent(
      host->GetAcceleratedWidget(), root_location, screen_location,
      std::move(task));
  return true;
}

// static
bool SendMouseEvents(MouseButton type,
                     int button_state,
                     int accelerator_state,
                     gfx::NativeWindow window_hint) {
  return SendMouseEventsNotifyWhenDone(type, button_state, base::OnceClosure(),
                                       accelerator_state, window_hint);
}

// static
bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                   int button_state,
                                   base::OnceClosure task,
                                   int accelerator_state,
                                   gfx::NativeWindow window_hint) {
  if (g_ozone_ui_controls_test_helper->SupportsScreenCoordinates()) {
    window_hint = nullptr;
  }

  gfx::Point mouse_loc_in_screen =
      aura::Env::GetInstance()->last_mouse_location();
  gfx::Point mouse_loc = mouse_loc_in_screen;
  aura::Window* root_window = RootWindowForPoint(mouse_loc, window_hint);
  if (root_window == nullptr) {
    return true;
  }

  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  if (screen_position_client) {
    screen_position_client->ConvertPointFromScreen(root_window, &mouse_loc);
  }

  g_ozone_ui_controls_test_helper->SendMouseEvent(
      root_window->GetHost()->GetAcceleratedWidget(), type, button_state,
      accelerator_state, mouse_loc, mouse_loc_in_screen, std::move(task));
  return true;
}

// static
bool SendMouseClick(MouseButton type, gfx::NativeWindow window_hint) {
  return SendMouseEvents(type, UP | DOWN, ui_controls::kNoAccelerator,
                         window_hint);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// static
bool SendTouchEvents(int action, int id, int x, int y) {
  return SendTouchEventsNotifyWhenDone(action, id, x, y, base::OnceClosure());
}

// static
bool SendTouchEventsNotifyWhenDone(int action,
                                   int id,
                                   int x,
                                   int y,
                                   base::OnceClosure task) {
  DCHECK(g_ozone_ui_controls_test_helper);
  gfx::Point screen_location(x, y);
  aura::Window* root_window;

  // Touch release events might not have coordinates that match any window, so
  // just use whichever window is on top.
  if (action & ui_controls::kTouchRelease) {
    root_window = TopRootWindow();
  } else {
    root_window = RootWindowForPoint(screen_location);
  }

  if (root_window == nullptr) {
    return true;
  }

  g_ozone_ui_controls_test_helper->SendTouchEvent(
      root_window->GetHost()->GetAcceleratedWidget(), action, id,
      screen_location, std::move(task));

  return true;
}

// static
void UpdateDisplaySync(const std::string& display_specs) {
  DCHECK(g_ozone_ui_controls_test_helper);
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  g_ozone_ui_controls_test_helper->UpdateDisplay(display_specs,
                                                 run_loop.QuitClosure());

  run_loop.Run();
}
#endif

#if BUILDFLAG(IS_LINUX)
// static
void ForceUseScreenCoordinatesOnce() {
  g_ozone_ui_controls_test_helper->ForceUseScreenCoordinatesOnce();
}
#endif

}  // namespace ui_controls

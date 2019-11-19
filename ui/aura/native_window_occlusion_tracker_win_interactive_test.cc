// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/native_window_occlusion_tracker_win.h"

#include <winuser.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_gdi_object.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/window_impl.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace aura {

// This class is used to verify expectations about occlusion state changes by
// adding instances of it as an observer of aura:Windows the tests create and
// checking that they get the expected call(s) to OnOcclusionStateChanged.
// The tests verify that the current state, when idle, is the expected state,
// because the state can be VISIBLE before it reaches the expected state.
class MockWindowTreeHostObserver : public WindowTreeHostObserver {
 public:
  explicit MockWindowTreeHostObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}
  ~MockWindowTreeHostObserver() override { EXPECT_FALSE(is_expecting_call()); }

  // WindowTreeHostObserver:
  void OnOcclusionStateChanged(WindowTreeHost* host,
                               Window::OcclusionState new_state) override {
    // Should only get notified when the occlusion state changes.
    EXPECT_NE(new_state, cur_state_);
    cur_state_ = new_state;
    if (expectation_ != Window::OcclusionState::UNKNOWN &&
        cur_state_ == expectation_) {
      EXPECT_FALSE(quit_closure_.is_null());
      std::move(quit_closure_).Run();
    }
  }

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  void set_expectation(Window::OcclusionState expectation) {
    expectation_ = expectation;
  }

  bool is_expecting_call() const { return expectation_ != cur_state_; }

 private:
  Window::OcclusionState expectation_ = Window::OcclusionState::UNKNOWN;
  Window::OcclusionState cur_state_ = Window::OcclusionState::UNKNOWN;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(MockWindowTreeHostObserver);
};

// Test wrapper around native window HWND.
class TestNativeWindow : public gfx::WindowImpl {
 public:
  TestNativeWindow() {}
  ~TestNativeWindow() override;

 private:
  // Overridden from gfx::WindowImpl:
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id) override {
    return FALSE;  // Results in DefWindowProc().
  }

  DISALLOW_COPY_AND_ASSIGN(TestNativeWindow);
};

TestNativeWindow::~TestNativeWindow() {
  if (hwnd())
    DestroyWindow(hwnd());
}

class NativeWindowOcclusionTrackerTest : public test::AuraTestBase {
 public:
  NativeWindowOcclusionTrackerTest() {
    // These interactive_ui_tests are not based on browser tests which would
    // normally handle initializing mojo. We can safely initialize mojo at the
    // start of the test here since a new process is launched for each test.
    mojo::core::Init();
  }
  void SetUp() override {
    if (gl::GetGLImplementation() == gl::kGLImplementationNone)
      gl::GLSurfaceTestSupport::InitializeOneOff();

    AuraTestBase::SetUp();
    ui::InitializeInputMethodForTesting();

    display::Screen::SetScreenInstance(test_screen());

    scoped_feature_list_.InitAndEnableFeature(
        features::kCalculateNativeWinOcclusion);
  }

  void SetNativeWindowBounds(HWND hwnd, const gfx::Rect& bounds) {
    RECT wr = bounds.ToRECT();
    AdjustWindowRectEx(&wr, GetWindowLong(hwnd, GWL_STYLE), FALSE,
                       GetWindowLong(hwnd, GWL_EXSTYLE));

    // Make sure to keep the window onscreen, as AdjustWindowRectEx() may have
    // moved part of it offscreen. But, if the original requested bounds are
    // offscreen, don't adjust the position.
    gfx::Rect window_bounds(wr);
    if (bounds.x() >= 0)
      window_bounds.set_x(std::max(0, window_bounds.x()));
    if (bounds.y() >= 0)
      window_bounds.set_y(std::max(0, window_bounds.y()));
    SetWindowPos(hwnd, HWND_TOP, window_bounds.x(), window_bounds.y(),
                 window_bounds.width(), window_bounds.height(),
                 SWP_NOREPOSITION);
    EXPECT_TRUE(UpdateWindow(hwnd));
  }

  HWND CreateNativeWindowWithBounds(const gfx::Rect& bounds) {
    native_win_ = std::make_unique<TestNativeWindow>();
    native_win_->set_window_style(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN);
    native_win_->Init(nullptr, bounds);
    HWND hwnd = native_win_->hwnd();
    SetNativeWindowBounds(hwnd, bounds);
    base::win::ScopedRegion region(CreateRectRgn(0, 0, 0, 0));
    if (GetWindowRgn(hwnd, region.get()) == COMPLEXREGION) {
      // On Windows 7, the newly created window has a complex region, which
      // means it will be ignored during the occlusion calculation. So, force
      // it to have a simple region so that we get test coverage on win 7.
      RECT bounding_rect;
      GetWindowRect(hwnd, &bounding_rect);
      base::win::ScopedRegion rectangular_region(
          CreateRectRgnIndirect(&bounding_rect));
      SetWindowRgn(hwnd, rectangular_region.get(), TRUE);
    }
    ShowWindow(hwnd, SW_SHOWNORMAL);
    EXPECT_TRUE(UpdateWindow(hwnd));
    return hwnd;
  }

  void CreateTrackedAuraWindowWithBounds(MockWindowTreeHostObserver* observer,
                                         gfx::Rect bounds) {
    host()->Show();
    host()->SetBoundsInPixels(bounds);
    host()->AddObserver(observer);

    Window* window = CreateNormalWindow(1, host()->window(), nullptr);
    window->SetBounds(bounds);

    Env::GetInstance()->GetWindowOcclusionTracker()->Track(window);
  }

  int GetNumVisibleRootWindows() {
    return NativeWindowOcclusionTrackerWin::GetOrCreateInstance()
        ->num_visible_root_windows_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestNativeWindow> native_win_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowOcclusionTrackerTest);
};

// Simple test completely covering an aura window with a native window.
TEST_F(NativeWindowOcclusionTrackerTest, SimpleOcclusion) {
  base::RunLoop run_loop;

  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100));
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  CreateNativeWindowWithBounds(gfx::Rect(0, 0, 100, 100));
  run_loop.Run();
  EXPECT_FALSE(observer.is_expecting_call());
}

// Simple test partially covering an aura window with a native window.
TEST_F(NativeWindowOcclusionTrackerTest, PartialOcclusion) {
  base::RunLoop run_loop;

  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100));
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  CreateNativeWindowWithBounds(gfx::Rect(0, 0, 50, 50));
  run_loop.Run();
  EXPECT_FALSE(observer.is_expecting_call());
}

// Simple test that a partly off screen aura window, with the on screen part
// occluded by a native window, is considered occluded.
TEST_F(NativeWindowOcclusionTrackerTest, OffscreenOcclusion) {
  base::RunLoop run_loop;

  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100));

  // Move the tracked window 50 pixels offscreen to the left.
  int screen_left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  SetWindowPos(host()->GetAcceleratedWidget(), HWND_TOP, screen_left - 50, 0,
               100, 100, SWP_NOZORDER | SWP_NOSIZE);

  // Create a native window that covers the onscreen part of the tracked window.
  CreateNativeWindowWithBounds(gfx::Rect(screen_left, 0, 50, 100));
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  run_loop.Run();
  EXPECT_FALSE(observer.is_expecting_call());
}

// Simple test with an aura window and native window that do not overlap.
TEST_F(NativeWindowOcclusionTrackerTest, SimpleVisible) {
  base::RunLoop run_loop;
  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100));
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  CreateNativeWindowWithBounds(gfx::Rect(200, 0, 100, 100));

  run_loop.Run();
  EXPECT_FALSE(observer.is_expecting_call());
}

// Simple test with a minimized aura window and native window.
TEST_F(NativeWindowOcclusionTrackerTest, SimpleHidden) {
  base::RunLoop run_loop;
  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100));
  CreateNativeWindowWithBounds(gfx::Rect(200, 0, 100, 100));
  // Iconify the tracked aura window and check that its occlusion state
  // is HIDDEN.
  CloseWindow(host()->GetAcceleratedWidget());
  observer.set_expectation(Window::OcclusionState::HIDDEN);
  run_loop.Run();
  EXPECT_FALSE(observer.is_expecting_call());
}

// Test that minimizing and restoring an app window results in the occlusion
// tracker re-registering for win events and detecting that a native window
// occludes the app window.
TEST_F(NativeWindowOcclusionTrackerTest, OcclusionAfterVisibilityToggle) {
  base::RunLoop run_loop;
  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100));
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  run_loop.Run();

  base::RunLoop run_loop2;
  observer.set_expectation(Window::OcclusionState::HIDDEN);
  observer.set_quit_closure(run_loop2.QuitClosure());
  // host()->window()->Hide() is needed to generate OnWindowVisibilityChanged
  // notifications.
  host()->window()->Hide();
  // This makes the window iconic.
  ::CloseWindow(host()->GetAcceleratedWidget());
  run_loop2.Run();
  // HIDDEN state is set synchronously by OnWindowVsiblityChanged notification,
  // before occlusion is calculated, so the above expectation will be met w/o an
  // occlusion calculation.
  // Loop until an occlusion calculation has run with no non-hidden app windows.

  do {
    // Need to pump events in order for UpdateOcclusionState to get called, and
    // update the number of non hidden root windows. When that number is 0,
    // occlusion has been calculated with no visible root windows.
    base::RunLoop().RunUntilIdle();
  } while (GetNumVisibleRootWindows() != 0);

  base::RunLoop run_loop3;
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  observer.set_quit_closure(run_loop3.QuitClosure());
  host()->window()->Show();
  // This opens the window made iconic above.
  OpenIcon(host()->GetAcceleratedWidget());
  run_loop3.Run();

  // Open a native window that occludes the visible app window.
  base::RunLoop run_loop4;
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  observer.set_quit_closure(run_loop4.QuitClosure());
  CreateNativeWindowWithBounds(gfx::Rect(0, 0, 100, 100));
  run_loop4.Run();
  EXPECT_FALSE(observer.is_expecting_call());
}

// Test that locking the screen causes visible windows to become occluded.
TEST_F(NativeWindowOcclusionTrackerTest, LockScreenVisibleOcclusion) {
  base::RunLoop run_loop;

  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100));
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  run_loop.Run();
  EXPECT_FALSE(observer.is_expecting_call());

  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  base::RunLoop run_loop2;
  observer.set_quit_closure(run_loop2.QuitClosure());
  // Unfortunately, this relies on knowing that NativeWindowOcclusionTracker
  // uses SessionChangeObserver to listen for WM_WTSSESSION_CHANGE messages, but
  // actually locking the screen isn't feasible.
  PostMessage(gfx::SingletonHwnd::GetInstance()->hwnd(), WM_WTSSESSION_CHANGE,
              WTS_SESSION_LOCK, 0);
  run_loop2.Run();
  EXPECT_FALSE(observer.is_expecting_call());
}

// Test that locking the screen leaves hidden windows as hidden.
TEST_F(NativeWindowOcclusionTrackerTest, LockScreenHiddenOcclusion) {
  base::RunLoop run_loop;

  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100));
  // Iconify the tracked aura window and check that its occlusion state
  // is HIDDEN.
  CloseWindow(host()->GetAcceleratedWidget());
  observer.set_expectation(Window::OcclusionState::HIDDEN);
  run_loop.Run();
  EXPECT_FALSE(observer.is_expecting_call());

  // Observer only gets notified on occlusion state changes, so force the
  // state to VISIBLE so that setting the state to hidden will trigger
  // a notification.
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::VISIBLE);

  observer.set_expectation(Window::OcclusionState::HIDDEN);
  base::RunLoop run_loop2;
  observer.set_quit_closure(run_loop2.QuitClosure());
  // Unfortunately, this relies on knowing that NativeWindowOcclusionTracker
  // uses SessionChangeObserver to listen for WM_WTSSESSION_CHANGE messages, but
  // actually locking the screen isn't feasible.
  PostMessage(gfx::SingletonHwnd::GetInstance()->hwnd(), WM_WTSSESSION_CHANGE,
              WTS_SESSION_LOCK, 0);
  run_loop2.Run();
  EXPECT_FALSE(observer.is_expecting_call());
}

}  // namespace aura

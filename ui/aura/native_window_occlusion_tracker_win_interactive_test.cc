// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "ui/aura/native_window_occlusion_tracker_win.h"

#include <winuser.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
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
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
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

  MockWindowTreeHostObserver(const MockWindowTreeHostObserver&) = delete;
  MockWindowTreeHostObserver& operator=(const MockWindowTreeHostObserver&) =
      delete;

  ~MockWindowTreeHostObserver() override { EXPECT_FALSE(is_expecting_call()); }

  // WindowTreeHostObserver:
  void OnOcclusionStateChanged(WindowTreeHost* host,
                               Window::OcclusionState new_state,
                               const SkRegion& occluded_region) override {
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
};

class MockWindowObserver : public WindowObserver {
 public:
  explicit MockWindowObserver(Window* window) : window_(window) {
    window_->AddObserver(this);
  }

  ~MockWindowObserver() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  void set_expectation(Window::OcclusionState expectation) {
    expectation_ = expectation;
  }

  // WindowObserver:
  void OnWindowOcclusionChanged(Window* window) override {
    if (expectation_ == window->GetOcclusionState()) {
      ASSERT_FALSE(quit_closure_.is_null());
      std::move(quit_closure_).Run();
    }
  }

  void OnWindowDestroyed(Window* window) override {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

 private:
  raw_ptr<Window> window_;
  Window::OcclusionState expectation_ = Window::OcclusionState::UNKNOWN;
  base::OnceClosure quit_closure_;
};

// Test wrapper around native window HWND.
class TestNativeWindow : public gfx::WindowImpl {
 public:
  TestNativeWindow() {}

  TestNativeWindow(const TestNativeWindow&) = delete;
  TestNativeWindow& operator=(const TestNativeWindow&) = delete;

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

  NativeWindowOcclusionTrackerTest(const NativeWindowOcclusionTrackerTest&) =
      delete;
  NativeWindowOcclusionTrackerTest& operator=(
      const NativeWindowOcclusionTrackerTest&) = delete;

  void SetUp() override {
    if (gl::GetGLImplementation() == gl::kGLImplementationNone)
      gl::GLSurfaceTestSupport::InitializeOneOff();

    scoped_feature_list_.InitWithFeatures(
        {features::kCalculateNativeWinOcclusion,
         features::kApplyNativeOccludedRegionToWindowTracker},
        {});

    AuraTestBase::SetUp();
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
    std::unique_ptr<TestNativeWindow> native_win =
        std::make_unique<TestNativeWindow>();
    native_win->set_window_style(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN);
    native_win->Init(nullptr, bounds);
    HWND hwnd = native_win->hwnd();
    SetNativeWindowBounds(hwnd, bounds);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    EXPECT_TRUE(UpdateWindow(hwnd));
    native_wins_.push_back(std::move(native_win));
    return hwnd;
  }

  Window* CreateTrackedAuraWindowWithBounds(
      MockWindowTreeHostObserver* observer,
      const gfx::Rect& bounds) {
    host()->Show();
    host()->SetBoundsInPixels(bounds);
    if (observer)
      host()->AddObserver(observer);

    Window* window = CreateNormalWindow(1, host()->window(), nullptr);
    window->SetBounds(gfx::Rect(bounds.size()));

    Env::GetInstance()->GetWindowOcclusionTracker()->Track(window);
    return window;
  }

  int GetNumVisibleRootWindows() {
    return NativeWindowOcclusionTrackerWin::GetOrCreateInstance()
        ->num_visible_root_windows_;
  }

  void MakeFullscreen(HWND hwnd) {
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    DWORD ex_style = GetWindowLong(hwnd, GWL_STYLE);
    SetWindowLong(hwnd, GWL_STYLE, style & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(hwnd, GWL_EXSTYLE,
                  ex_style & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                               WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                   &monitor_info);
    gfx::Rect window_rect(monitor_info.rcMonitor);
    SetWindowPos(hwnd, nullptr, window_rect.x(), window_rect.y(),
                 window_rect.width(), window_rect.height(),
                 SWP_FRAMECHANGED | SWP_ASYNCWINDOWPOS);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<std::unique_ptr<TestNativeWindow>> native_wins_;
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
  host()->RemoveObserver(&observer);
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
  host()->RemoveObserver(&observer);
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
  host()->RemoveObserver(&observer);
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
  host()->RemoveObserver(&observer);
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
  host()->RemoveObserver(&observer);
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
  host()->RemoveObserver(&observer);
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
  DWORD current_session_id = 0;
  ProcessIdToSessionId(::GetCurrentProcessId(), &current_session_id);
  PostMessage(gfx::SingletonHwnd::GetInstance()->hwnd(), WM_WTSSESSION_CHANGE,
              WTS_SESSION_LOCK, current_session_id);
  run_loop2.Run();
  EXPECT_FALSE(observer.is_expecting_call());
  host()->RemoveObserver(&observer);
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
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::VISIBLE, {});

  observer.set_expectation(Window::OcclusionState::HIDDEN);
  base::RunLoop run_loop2;
  observer.set_quit_closure(run_loop2.QuitClosure());
  // Unfortunately, this relies on knowing that NativeWindowOcclusionTracker
  // uses SessionChangeObserver to listen for WM_WTSSESSION_CHANGE messages, but
  // actually locking the screen isn't feasible.
  DWORD current_session_id = 0;
  ProcessIdToSessionId(::GetCurrentProcessId(), &current_session_id);
  PostMessage(gfx::SingletonHwnd::GetInstance()->hwnd(), WM_WTSSESSION_CHANGE,
              WTS_SESSION_LOCK, current_session_id);
  run_loop2.Run();
  EXPECT_FALSE(observer.is_expecting_call());
  host()->RemoveObserver(&observer);
}

// Test that locking the screen from a different session doesn't mark window
// as occluded.
TEST_F(NativeWindowOcclusionTrackerTest, LockScreenDifferentSession) {
  base::RunLoop run_loop;

  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100));
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  run_loop.Run();
  EXPECT_FALSE(observer.is_expecting_call());

  // Observer only gets notified on occlusion state changes, so force the
  // state to OCCLUDED so that setting the state to VISIBLE will trigger
  // a notification.
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::OCCLUDED, {});

  // Generate a session change lock screen with a session id that's not
  // |current_session_id|.
  DWORD current_session_id = 0;
  ProcessIdToSessionId(::GetCurrentProcessId(), &current_session_id);
  PostMessage(gfx::SingletonHwnd::GetInstance()->hwnd(), WM_WTSSESSION_CHANGE,
              WTS_SESSION_LOCK, current_session_id + 1);

  observer.set_expectation(Window::OcclusionState::VISIBLE);
  base::RunLoop run_loop2;
  observer.set_quit_closure(run_loop2.QuitClosure());
  // Create a native window to trigger occlusion calculation.
  CreateNativeWindowWithBounds(gfx::Rect(0, 0, 50, 50));
  run_loop2.Run();
  EXPECT_FALSE(observer.is_expecting_call());
  host()->RemoveObserver(&observer);
}

// Test that display off & on power state notification causes visible windows to
// become occluded, then visible.
TEST_F(NativeWindowOcclusionTrackerTest, DisplayOnOffHandling) {
  base::RunLoop run_loop;

  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100));
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  run_loop.Run();
  EXPECT_FALSE(observer.is_expecting_call());

  NativeWindowOcclusionTrackerWin* occlusion_tracker =
      NativeWindowOcclusionTrackerWin::GetOrCreateInstance();

  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  base::RunLoop run_loop2;
  observer.set_quit_closure(run_loop2.QuitClosure());

  // Turning display off and on isn't feasible, so send a notification.
  occlusion_tracker->OnDisplayStateChanged(/*display_on=*/false);
  run_loop2.Run();
  EXPECT_FALSE(observer.is_expecting_call());

  observer.set_expectation(Window::OcclusionState::VISIBLE);
  base::RunLoop run_loop3;
  observer.set_quit_closure(run_loop3.QuitClosure());
  occlusion_tracker->OnDisplayStateChanged(/*display_on=*/true);
  run_loop3.Run();
  EXPECT_FALSE(observer.is_expecting_call());
  host()->RemoveObserver(&observer);
}

// Verifies that a window is not occluded if the only window occluding it is
// being moved/dragged.
//
// TODO(crbug.com/40801894): Flaky on Windows.
TEST_F(NativeWindowOcclusionTrackerTest,
       DISABLED_MovingWindowNotConsideredInCalculations) {
  // Needed as this test triggers a native nested message loop.
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop
      allow_nesting;

  // Create the initial window.
  base::RunLoop run_loop;
  MockWindowTreeHostObserver observer(run_loop.QuitClosure());
  CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(40, 40, 100, 100));
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  run_loop.Run();
  EXPECT_FALSE(observer.is_expecting_call());

  // Creates a new window that obscures the initial window.
  CreateNativeWindowWithBounds(gfx::Rect(0, 0, 200, 200));
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  base::RunLoop run_loop2;
  observer.set_quit_closure(run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_FALSE(observer.is_expecting_call());

  // Start a window move loop. As windows being moved/dragged are not considered
  // during occlusion calculation, the initial window should become visible.
  base::RunLoop run_loop3(base::RunLoop::Type::kNestableTasksAllowed);
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  observer.set_quit_closure(base::BindLambdaForTesting([&] {
    // Release the mouse, which should make the initial window occluded.
    observer.set_expectation(Window::OcclusionState::OCCLUDED);
    observer.set_quit_closure(run_loop3.QuitClosure());
    ASSERT_TRUE(
        ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::UP));
  }));
  ASSERT_TRUE(ui_controls::SendMouseMove(40, 8));
  ASSERT_TRUE(
      ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::DOWN));
  run_loop3.Run();
  EXPECT_FALSE(observer.is_expecting_call());

  host()->RemoveObserver(&observer);
}

// Test that a maximized aura window that is covered by a fullscreen window
// is marked as occluded. TODO(crbug.com/40833493): Fix flakiness.
TEST_F(NativeWindowOcclusionTrackerTest,
       DISABLED_MaximizedOccludedByFullscreenWindow) {
  // Create an aura window that is maximized.
  base::RunLoop run_loop1;
  MockWindowTreeHostObserver observer(run_loop1.QuitClosure());
  HWND hwnd_aura_window_maximized =
      CreateTrackedAuraWindowWithBounds(&observer, gfx::Rect(0, 0, 100, 100))
          ->GetHost()
          ->GetAcceleratedWidget();
  ShowWindow(hwnd_aura_window_maximized, SW_SHOWMAXIMIZED);
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  run_loop1.Run();
  EXPECT_FALSE(observer.is_expecting_call());
  // Create a fullscreen native window that occludes the aura window.
  base::RunLoop run_loop2;
  observer.set_quit_closure(run_loop2.QuitClosure());
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  HWND hwnd_native_window =
      CreateNativeWindowWithBounds(gfx::Rect(0, 0, 100, 100));
  MakeFullscreen(hwnd_native_window);
  run_loop2.Run();
  EXPECT_FALSE(observer.is_expecting_call());
  host()->RemoveObserver(&observer);
}

TEST_F(NativeWindowOcclusionTrackerTest, OccludedRegionSimple) {
  Window* tracked_aura_window =
      CreateTrackedAuraWindowWithBounds(nullptr, gfx::Rect(20, 20, 200, 200));
  tracked_aura_window->SetBounds(gfx::Rect(0, 0, 60, 60));

  MockWindowObserver observer(tracked_aura_window);
  base::RunLoop run_loop;
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  observer.set_quit_closure(run_loop.QuitClosure());
  HWND obscuring_hwnd =
      CreateNativeWindowWithBounds(gfx::Rect(20, 20, 110, 110));
  run_loop.Run();
  EXPECT_EQ(Window::OcclusionState::OCCLUDED,
            tracked_aura_window->GetOcclusionState());

  base::RunLoop run_loop2;
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  observer.set_quit_closure(run_loop2.QuitClosure());
  tracked_aura_window->SetBounds(gfx::Rect(160, 160, 20, 20));
  run_loop2.Run();
  EXPECT_EQ(Window::OcclusionState::VISIBLE,
            tracked_aura_window->GetOcclusionState());

  base::RunLoop run_loop3;
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  observer.set_quit_closure(run_loop3.QuitClosure());
  SetNativeWindowBounds(obscuring_hwnd, gfx::Rect(140, 140, 110, 110));
  run_loop3.Run();
  EXPECT_EQ(Window::OcclusionState::OCCLUDED,
            tracked_aura_window->GetOcclusionState());
}

TEST_F(NativeWindowOcclusionTrackerTest, OccludedRegionComplex) {
  Window* tracked_aura_window =
      CreateTrackedAuraWindowWithBounds(nullptr, gfx::Rect(20, 20, 200, 200));
  tracked_aura_window->SetBounds(gfx::Rect(0, 0, 60, 60));

  MockWindowObserver observer(tracked_aura_window);
  base::RunLoop run_loop;
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  observer.set_quit_closure(run_loop.QuitClosure());
  CreateNativeWindowWithBounds(gfx::Rect(20, 20, 110, 110));
  run_loop.Run();
  EXPECT_EQ(Window::OcclusionState::OCCLUDED,
            tracked_aura_window->GetOcclusionState());

  base::RunLoop run_loop2;
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  observer.set_quit_closure(run_loop2.QuitClosure());
  tracked_aura_window->SetBounds(gfx::Rect(160, 160, 20, 20));
  run_loop2.Run();
  EXPECT_EQ(Window::OcclusionState::VISIBLE,
            tracked_aura_window->GetOcclusionState());

  base::RunLoop run_loop3;
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  observer.set_quit_closure(run_loop3.QuitClosure());
  CreateNativeWindowWithBounds(gfx::Rect(140, 140, 110, 110));
  run_loop3.Run();
  EXPECT_EQ(Window::OcclusionState::OCCLUDED,
            tracked_aura_window->GetOcclusionState());
}

class NativeWindowOcclusionTrackerTestWithDpi2
    : public NativeWindowOcclusionTrackerTest {
 public:
  // NativeWindowOcclusionTrackerTest:
  void SetUp() override {
    display::Display::SetForceDeviceScaleFactor(2.0);
    NativeWindowOcclusionTrackerTest::SetUp();
  }
};

TEST_F(NativeWindowOcclusionTrackerTestWithDpi2, OccludedRegionSimple) {
  Window* tracked_aura_window =
      CreateTrackedAuraWindowWithBounds(nullptr, gfx::Rect(20, 20, 200, 200));
  tracked_aura_window->SetBounds(gfx::Rect(0, 0, 30, 30));

  MockWindowObserver observer(tracked_aura_window);
  base::RunLoop run_loop;
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  observer.set_quit_closure(run_loop.QuitClosure());
  HWND obscuring_hwnd =
      CreateNativeWindowWithBounds(gfx::Rect(20, 20, 110, 110));
  run_loop.Run();
  EXPECT_EQ(Window::OcclusionState::OCCLUDED,
            tracked_aura_window->GetOcclusionState());

  base::RunLoop run_loop2;
  observer.set_expectation(Window::OcclusionState::VISIBLE);
  observer.set_quit_closure(run_loop2.QuitClosure());
  tracked_aura_window->SetBounds(gfx::Rect(80, 80, 20, 20));
  run_loop2.Run();
  EXPECT_EQ(Window::OcclusionState::VISIBLE,
            tracked_aura_window->GetOcclusionState());

  base::RunLoop run_loop3;
  observer.set_expectation(Window::OcclusionState::OCCLUDED);
  observer.set_quit_closure(run_loop3.QuitClosure());
  SetNativeWindowBounds(obscuring_hwnd, gfx::Rect(140, 140, 110, 110));
  run_loop3.Run();
  EXPECT_EQ(Window::OcclusionState::OCCLUDED,
            tracked_aura_window->GetOcclusionState());
}

}  // namespace aura

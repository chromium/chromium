// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/native_window_occlusion_tracker_win.h"

#include <windows.h>

#include <memory>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/win/window_impl.h"

namespace aura {

namespace {

constexpr gfx::Rect kBounds(100, 100, 300, 200);
constexpr gfx::Rect kMovedBounds(110, 100, 300, 200);
constexpr gfx::Rect kResizedBounds(100, 100, 310, 200);

// A native window that handles all messages with DefWindowProc().
class TestNativeWindow : public gfx::WindowImpl {
 public:
  TestNativeWindow(HWND parent,
                   DWORD style,
                   DWORD ex_style,
                   const gfx::Rect& bounds) {
    set_window_style(style);
    set_window_ex_style(ex_style);
    Init(parent, bounds);
  }

  TestNativeWindow(const TestNativeWindow&) = delete;
  TestNativeWindow& operator=(const TestNativeWindow&) = delete;

  ~TestNativeWindow() override {
    if (hwnd()) {
      ::DestroyWindow(hwnd());
    }
  }

 private:
  // gfx::WindowImpl:
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id) override {
    return FALSE;
  }
};

// Creates a top-level window with `kBounds`, and shows it without activating
// it. Unless `ex_style` prevents it, the window can occlude other windows.
std::unique_ptr<TestNativeWindow> CreateTopLevelWindow(DWORD ex_style = 0) {
  auto window = std::make_unique<TestNativeWindow>(nullptr, WS_OVERLAPPEDWINDOW,
                                                   ex_style, kBounds);
  ::ShowWindow(window->hwnd(), SW_SHOWNOACTIVATE);
  return window;
}

void SetWindowBounds(HWND hwnd, const gfx::Rect& bounds) {
  ::SetWindowPos(hwnd, nullptr, bounds.x(), bounds.y(), bounds.width(),
                 bounds.height(), SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace

// Tests how WindowOcclusionCalculator handles EVENT_OBJECT_LOCATIONCHANGE, with
// kFilterNativeWinOcclusionLocationChanges enabled if the param is true. The
// calculator runs on the main thread. Instead of registering event hooks and
// enumerating windows, the tests set up the state an occlusion calculation
// leaves behind for their windows, and call the event hook callback directly,
// so other windows on the desktop don't affect them.
class NativeWindowOcclusionTrackerTest : public testing::TestWithParam<bool> {
 public:
  NativeWindowOcclusionTrackerTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kFilterNativeWinOcclusionLocationChanges, GetParam());
  }

  void SetUp() override {
    WindowOcclusionCalculator::CreateInstance(
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(), /*tracker=*/nullptr);
  }

  void TearDown() override {
    base::WaitableEvent done_event;
    WindowOcclusionCalculator::DeleteInstanceForTesting(&done_event);
    done_event.Wait();
  }

 protected:
  bool filter_location_changes() const { return GetParam(); }

  // Records `hwnd` like an occlusion calculation does: if it can occlude other
  // windows, remembers its bounds. Returns whether it can.
  bool RecordInOcclusionCalculation(HWND hwnd) {
    gfx::Rect window_rect;
    if (!calculator()->WindowCanOccludeOtherWindowsOnCurrentVirtualDesktop(
            hwnd, &window_rect)) {
      return false;
    }
    calculator()->occluding_window_rects_[hwnd] = window_rect;
    return true;
  }

  // Makes `hwnd` one of the tracked root windows.
  void TrackRootWindow(HWND hwnd) {
    calculator()->root_window_hwnds_occlusion_state_[hwnd] = {};
  }

  // Delivers EVENT_OBJECT_LOCATIONCHANGE for `hwnd` like the event hook does,
  // and returns whether that scheduled an occlusion calculation. Cancels the
  // calculation, so that the next call starts from the same state.
  bool LocationChangeSchedulesCalculation(HWND hwnd) {
    WindowOcclusionCalculator::EventHookCallback(
        /*hWinEventHook=*/nullptr, EVENT_OBJECT_LOCATIONCHANGE, hwnd,
        OBJID_WINDOW, CHILDID_SELF, /*dwEventThread=*/0, /*dwmsEventTime=*/0);
    // The callback handles the event in at most one task posted to the
    // calculator's sequence, which runs before this one.
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    const bool scheduled = calculator()->occlusion_update_timer_.IsRunning();
    calculator()->occlusion_update_timer_.Stop();
    return scheduled;
  }

 private:
  using WindowOcclusionCalculator =
      NativeWindowOcclusionTrackerWin::WindowOcclusionCalculator;

  static WindowOcclusionCalculator* calculator() {
    return WindowOcclusionCalculator::GetInstance();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  // Mock time keeps scheduled occlusion calculations from running.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// A child window can only move within its parent, so its location changes
// can't change occlusion, even if it actually moved.
TEST_P(NativeWindowOcclusionTrackerTest, ChildWindowLocationChange) {
  std::unique_ptr<TestNativeWindow> parent = CreateTopLevelWindow();
  ASSERT_TRUE(RecordInOcclusionCalculation(parent->hwnd()));
  TestNativeWindow child(parent->hwnd(), WS_CHILD | WS_VISIBLE, 0,
                         gfx::Rect(10, 10, 50, 50));

  SetWindowBounds(child.hwnd(), gfx::Rect(20, 20, 50, 50));
  EXPECT_EQ(LocationChangeSchedulesCalculation(child.hwnd()),
            !filter_location_changes());
}

// Location changes that don't move or resize a window that can occlude other
// windows, e.g., the ones layered windows raise when they update their
// contents, can't change occlusion.
TEST_P(NativeWindowOcclusionTrackerTest,
       UnchangedOccludingWindowLocationChange) {
  std::unique_ptr<TestNativeWindow> window = CreateTopLevelWindow();
  ASSERT_TRUE(RecordInOcclusionCalculation(window->hwnd()));

  EXPECT_EQ(LocationChangeSchedulesCalculation(window->hwnd()),
            !filter_location_changes());
}

TEST_P(NativeWindowOcclusionTrackerTest, MovedOccludingWindowLocationChange) {
  std::unique_ptr<TestNativeWindow> window = CreateTopLevelWindow();
  ASSERT_TRUE(RecordInOcclusionCalculation(window->hwnd()));

  SetWindowBounds(window->hwnd(), kMovedBounds);
  EXPECT_TRUE(LocationChangeSchedulesCalculation(window->hwnd()));
}

TEST_P(NativeWindowOcclusionTrackerTest, ResizedOccludingWindowLocationChange) {
  std::unique_ptr<TestNativeWindow> window = CreateTopLevelWindow();
  ASSERT_TRUE(RecordInOcclusionCalculation(window->hwnd()));

  SetWindowBounds(window->hwnd(), kResizedBounds);
  EXPECT_TRUE(LocationChangeSchedulesCalculation(window->hwnd()));
}

// Location changes of a window that can't occlude other windows, e.g., a
// click-through overlay, can't change occlusion, unless the window has become
// able to occlude other windows.
TEST_P(NativeWindowOcclusionTrackerTest, NonOccludingWindowLocationChange) {
  std::unique_ptr<TestNativeWindow> window =
      CreateTopLevelWindow(WS_EX_TRANSPARENT);
  ASSERT_FALSE(RecordInOcclusionCalculation(window->hwnd()));

  SetWindowBounds(window->hwnd(), kMovedBounds);
  EXPECT_EQ(LocationChangeSchedulesCalculation(window->hwnd()),
            !filter_location_changes());

  ::SetWindowLong(
      window->hwnd(), GWL_EXSTYLE,
      ::GetWindowLong(window->hwnd(), GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
  EXPECT_TRUE(LocationChangeSchedulesCalculation(window->hwnd()));
}

// Minimizing a window that could occlude other windows raises a location
// change, and can change occlusion, since the window can't occlude other
// windows anymore.
TEST_P(NativeWindowOcclusionTrackerTest,
       MinimizedOccludingWindowLocationChange) {
  std::unique_ptr<TestNativeWindow> window = CreateTopLevelWindow();
  ASSERT_TRUE(RecordInOcclusionCalculation(window->hwnd()));

  ::ShowWindow(window->hwnd(), SW_SHOWMINNOACTIVE);
  EXPECT_TRUE(LocationChangeSchedulesCalculation(window->hwnd()));
}

// Events are delivered asynchronously, so a window that could occlude other
// windows may have been destroyed by the time its location change arrives.
TEST_P(NativeWindowOcclusionTrackerTest,
       DestroyedOccludingWindowLocationChange) {
  std::unique_ptr<TestNativeWindow> window = CreateTopLevelWindow();
  const HWND hwnd = window->hwnd();
  ASSERT_TRUE(RecordInOcclusionCalculation(hwnd));

  window.reset();
  EXPECT_TRUE(LocationChangeSchedulesCalculation(hwnd));
}

// The occlusion state of a tracked root window depends on its own bounds, so
// its location changes always trigger an occlusion calculation.
TEST_P(NativeWindowOcclusionTrackerTest, TrackedRootWindowLocationChange) {
  std::unique_ptr<TestNativeWindow> window = CreateTopLevelWindow();
  ASSERT_TRUE(RecordInOcclusionCalculation(window->hwnd()));
  TrackRootWindow(window->hwnd());

  EXPECT_TRUE(LocationChangeSchedulesCalculation(window->hwnd()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         NativeWindowOcclusionTrackerTest,
                         testing::Bool());

}  // namespace aura

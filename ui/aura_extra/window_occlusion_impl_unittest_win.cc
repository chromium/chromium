// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_extra/window_occlusion_impl_win.h"

#include "base/win/scoped_gdi_object.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace aura_extra {

// A single set of arguments that are passed to WindowEvaluator::Evaluate().
// Used to mock calls to WindowEvaluator::Evaluate().
struct EvaluatorArgs {
  bool is_relevant;
  gfx::Rect window_rect;
  HWND hwnd;
};

// Iterates over a provided set of mock windows and their properties.
class MockNativeWindowIterator : public NativeWindowIterator {
 public:
  MockNativeWindowIterator(
      const std::vector<EvaluatorArgs>& evaluator_args_list)
      : args_list_(evaluator_args_list) {}

  void Iterate(WindowEvaluator* evaluator) override {
    for (EvaluatorArgs args : args_list_) {
      if (!evaluator->EvaluateWindow(args.is_relevant, args.window_rect,
                                     args.hwnd))
        return;
    }
  }

 private:
  std::vector<EvaluatorArgs> args_list_;

  DISALLOW_COPY_AND_ASSIGN(MockNativeWindowIterator);
};

// Test implementation of WindowBoundsDelegate using a flat_map of aura::Window
// to gfx::Rect.
class MockWindowBoundsDelegateImpl : public WindowBoundsDelegate {
 public:
  MockWindowBoundsDelegateImpl() = default;

  // WindowBoundsDelegate implementation:
  gfx::Rect GetBoundsInPixels(aura::WindowTreeHost* window) override {
    return root_window_bounds_[window];
  }

  void AddWindowWithBounds(aura::WindowTreeHost* window,
                           const gfx::Rect& window_bounds_in_pixels) {
    root_window_bounds_[window] = window_bounds_in_pixels;
  }

 private:
  base::flat_map<aura::WindowTreeHost*, gfx::Rect> root_window_bounds_;

  DISALLOW_COPY_AND_ASSIGN(MockWindowBoundsDelegateImpl);
};

// The int argument here is an offset in pixels for tests that need to offset
// windows. This allows for variable offsets in the parameterized tests.
using OffsetAndBoundsPair = std::pair<int, gfx::Rect>;

class WindowOcclusionWinTest
    : public aura::test::AuraTestBase,
      public ::testing::WithParamInterface<OffsetAndBoundsPair> {
 public:
  WindowOcclusionWinTest() {}

  void TearDown() override {
    Clear();
    aura::test::AuraTestBase::TearDown();
  }

  aura::WindowTreeHost* AddRootAuraWindowWithBounds(const gfx::Rect& bounds) {
    std::unique_ptr<aura::WindowTreeHost> window_tree_host =
        aura::WindowTreeHost::Create(ui::PlatformWindowInitProperties{bounds});
    window_tree_host->window()->Show();

    EvaluatorArgs args{true, bounds, window_tree_host->GetAcceleratedWidget()};
    evaluator_args_list_.push_back(args);

    mock_bounds_delegate_->AddWindowWithBounds(window_tree_host.get(), bounds);

    aura::WindowTreeHost* host = window_tree_host.get();
    window_tree_hosts_.push_back(std::move(window_tree_host));
    return host;
  }

  void AddMockNativeWindowWithBounds(gfx::Rect bounds) {
    EvaluatorArgs args{true, bounds, 0};
    evaluator_args_list_.push_back(args);
  }

  base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState>
  ComputeOcclusion() {
    std::unique_ptr<NativeWindowIterator> iterator =
        std::make_unique<MockNativeWindowIterator>(evaluator_args_list_);
    std::vector<aura::WindowTreeHost*> window_tree_hosts;

    for (auto& host : window_tree_hosts_)
      window_tree_hosts.push_back(host.get());

    return ComputeNativeWindowOcclusionStatusImpl(
        window_tree_hosts, std::move(iterator),
        std::unique_ptr<WindowBoundsDelegate>(mock_bounds_delegate_.release()));
  }

  void Clear() {
    evaluator_args_list_.clear();
    window_tree_hosts_.clear();
  }

 private:
  std::vector<EvaluatorArgs> evaluator_args_list_;

  std::vector<std::unique_ptr<aura::WindowTreeHost>> window_tree_hosts_;
  std::unique_ptr<MockWindowBoundsDelegateImpl> mock_bounds_delegate_ =
      std::make_unique<MockWindowBoundsDelegateImpl>();

  DISALLOW_COPY_AND_ASSIGN(WindowOcclusionWinTest);
};

// An aura window completely covered by a native window should be occluded.
TEST_P(WindowOcclusionWinTest, SimpleOccluded) {
  OffsetAndBoundsPair param = GetParam();
  AddMockNativeWindowWithBounds(param.second);
  aura::WindowTreeHost* window = AddRootAuraWindowWithBounds(param.second);

  base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState> result =
      ComputeOcclusion();

  EXPECT_EQ(result.size(), 1U);
  ASSERT_TRUE(base::Contains(result, window));
  EXPECT_EQ(result[window], aura::Window::OcclusionState::OCCLUDED);
}

// An aura window not occluded at all by a native window should be visible.
TEST_P(WindowOcclusionWinTest, SimpleVisible) {
  OffsetAndBoundsPair param = GetParam();
  aura::WindowTreeHost* window = AddRootAuraWindowWithBounds(param.second);
  AddMockNativeWindowWithBounds(param.second);

  base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState> result =
      ComputeOcclusion();

  EXPECT_EQ(result.size(), 1U);
  ASSERT_TRUE(base::Contains(result, window));
  EXPECT_EQ(result[window], aura::Window::OcclusionState::VISIBLE);
  Clear();
}

// An aura window occluded by an aura window should be occluded.
TEST_P(WindowOcclusionWinTest, OccludedByAuraWindow) {
  OffsetAndBoundsPair param = GetParam();
  aura::WindowTreeHost* window1 = AddRootAuraWindowWithBounds(param.second);
  aura::WindowTreeHost* window2 = AddRootAuraWindowWithBounds(param.second);

  std::vector<aura::WindowTreeHost*> windows({window1, window2});

  base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState> result =
      ComputeOcclusion();

  EXPECT_EQ(result.size(), 2U);

  ASSERT_TRUE(base::Contains(result, window1));
  EXPECT_EQ(result[window1], aura::Window::OcclusionState::VISIBLE);

  ASSERT_TRUE(base::Contains(result, window2));
  EXPECT_EQ(result[window2], aura::Window::OcclusionState::OCCLUDED);
}

// An aura window occluded by two native windows should be occluded.
TEST_P(WindowOcclusionWinTest, OccludedByMultipleWindows) {
  OffsetAndBoundsPair param = GetParam();

  gfx::Rect left_half = param.second;
  left_half.Offset(-left_half.width() / 2, 0);

  gfx::Rect right_half = param.second;
  right_half.Offset(right_half.width() / 2, 0);

  AddMockNativeWindowWithBounds(left_half);
  AddMockNativeWindowWithBounds(right_half);
  aura::WindowTreeHost* window = AddRootAuraWindowWithBounds(param.second);

  base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState> result =
      ComputeOcclusion();

  EXPECT_EQ(result.size(), 1U);
  ASSERT_TRUE(base::Contains(result, window));
  EXPECT_EQ(result[window], aura::Window::OcclusionState::OCCLUDED);
}

// An aura window partially occluded by an aura window should be visible.
TEST_P(WindowOcclusionWinTest, PartiallyOverlappedAuraWindows) {
  OffsetAndBoundsPair param = GetParam();
  aura::WindowTreeHost* window1 = AddRootAuraWindowWithBounds(param.second);

  gfx::Rect offset_bounds = param.second;
  offset_bounds.Offset(param.first, param.first);
  aura::WindowTreeHost* window2 = AddRootAuraWindowWithBounds(offset_bounds);

  base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState> result =
      ComputeOcclusion();

  EXPECT_EQ(result.size(), 2U);

  ASSERT_TRUE(base::Contains(result, window1));
  EXPECT_EQ(result[window1], aura::Window::OcclusionState::VISIBLE);

  ASSERT_TRUE(base::Contains(result, window2));
  EXPECT_EQ(result[window2], aura::Window::OcclusionState::VISIBLE);
}

// An aura window partially occluded by a native window should be visible.
TEST_P(WindowOcclusionWinTest, PartiallyOverlappedWindows) {
  OffsetAndBoundsPair param = GetParam();
  aura::WindowTreeHost* window = AddRootAuraWindowWithBounds(param.second);

  gfx::Rect offset_bounds = param.second;
  offset_bounds.Offset(param.first, param.first);
  AddMockNativeWindowWithBounds(offset_bounds);

  base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState> result =
      ComputeOcclusion();

  EXPECT_EQ(result.size(), 1U);

  ASSERT_TRUE(base::Contains(result, window));
  EXPECT_EQ(result[window], aura::Window::OcclusionState::VISIBLE);
}

// If for some reason the bounds of an aura::Window are empty, this signals some
// sort of failure in the call to GetWindowRect() This tests that in this case
// the window is marked as aura::Window::OcclusionState::VISIBLE to avoid
// falsely marking it as occluded.
TEST_P(WindowOcclusionWinTest, EmptyWindowIsVisible) {
  aura::WindowTreeHost* window =
      AddRootAuraWindowWithBounds(gfx::Rect(0, 0, 0, 0));

  base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState> result =
      ComputeOcclusion();

  EXPECT_EQ(result.size(), 1U);

  ASSERT_TRUE(base::Contains(result, window));
  EXPECT_EQ(result[window], aura::Window::OcclusionState::VISIBLE);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */
                         ,
                         WindowOcclusionWinTest,
                         ::testing::Values(
                             OffsetAndBoundsPair(5, gfx::Rect(0, 0, 100, 100)),
                             OffsetAndBoundsPair(10, gfx::Rect(0, 0, 100, 200)),
                             OffsetAndBoundsPair(15, gfx::Rect(0, 0, 200, 100)),
                             OffsetAndBoundsPair(20, gfx::Rect(0, 0, 200, 200)),
                             OffsetAndBoundsPair(25,
                                                 gfx::Rect(0, 50, 100, 100)),
                             OffsetAndBoundsPair(50,
                                                 gfx::Rect(0, 50, 100, 200)),
                             OffsetAndBoundsPair(75,
                                                 gfx::Rect(0, 50, 200, 100)),
                             OffsetAndBoundsPair(100,
                                                 gfx::Rect(0, 50, 200, 200)),
                             OffsetAndBoundsPair(125,
                                                 gfx::Rect(100, 0, 100, 100)),
                             OffsetAndBoundsPair(150,
                                                 gfx::Rect(100, 0, 100, 200)),
                             OffsetAndBoundsPair(200,
                                                 gfx::Rect(100, 0, 200, 100)),
                             OffsetAndBoundsPair(250,
                                                 gfx::Rect(100, 0, 200, 200)),
                             OffsetAndBoundsPair(300,
                                                 gfx::Rect(100, 50, 100, 100)),
                             OffsetAndBoundsPair(400,
                                                 gfx::Rect(100, 50, 100, 200)),
                             OffsetAndBoundsPair(500,
                                                 gfx::Rect(100, 50, 200, 100)),
                             OffsetAndBoundsPair(
                                 750,
                                 gfx::Rect(100, 50, 200, 200))));

class WindowFitnessFunctionTest : public testing::Test {
 public:
  HWND CreateNativeWindow(gfx::Rect bounds) {
    HWND hwnd = CreateWindow(L"STATIC", L"TestWindow", WS_OVERLAPPED,
                             bounds.x(), bounds.y(), bounds.width(),
                             bounds.height(), (HWND)NULL, NULL, NULL, NULL);

    return hwnd;
  }

  // Adds the windows style |style| to |hwnd|.
  void AddStyle(HWND hwnd, int style_type, DWORD style) {
    SetWindowLong(hwnd, style_type, GetWindowLong(hwnd, style_type) | style);
  }

  void RemoveStyle(HWND hwnd, int style_type, DWORD style) {
    SetWindowLong(hwnd, style_type, GetWindowLong(hwnd, style_type) & ~style);
  }
};

TEST_F(WindowFitnessFunctionTest, FitnessTest) {
  HWND hwnd = CreateNativeWindow(gfx::Rect(0, 0, 100, 100));
  gfx::Rect rect;

  // The window doesn't have the WS_VISIBLE style yet, so it should not pass.
  EXPECT_FALSE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  AddStyle(hwnd, GWL_STYLE, WS_VISIBLE);
  EXPECT_TRUE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  AddStyle(hwnd, GWL_STYLE, WS_MINIMIZE);
  EXPECT_FALSE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  RemoveStyle(hwnd, GWL_STYLE, WS_MINIMIZE);
  EXPECT_TRUE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  AddStyle(hwnd, GWL_EXSTYLE, WS_EX_TRANSPARENT);
  EXPECT_FALSE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  RemoveStyle(hwnd, GWL_EXSTYLE, WS_EX_TRANSPARENT);
  EXPECT_TRUE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  AddStyle(hwnd, GWL_EXSTYLE, WS_EX_TOOLWINDOW);
  EXPECT_FALSE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  RemoveStyle(hwnd, GWL_EXSTYLE, WS_EX_TOOLWINDOW);
  EXPECT_TRUE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  AddStyle(hwnd, GWL_EXSTYLE, WS_EX_LAYERED);
  SetLayeredWindowAttributes(hwnd, RGB(10, 10, 10), NULL, LWA_COLORKEY);
  EXPECT_FALSE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  RemoveStyle(hwnd, GWL_EXSTYLE, WS_EX_LAYERED);
  EXPECT_TRUE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  AddStyle(hwnd, GWL_EXSTYLE, WS_EX_LAYERED);
  SetLayeredWindowAttributes(hwnd, NULL, 0, LWA_ALPHA);
  EXPECT_FALSE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  RemoveStyle(hwnd, GWL_EXSTYLE, WS_EX_LAYERED);
  EXPECT_TRUE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  AddStyle(hwnd, GWL_EXSTYLE, WS_EX_LAYERED);
  SetLayeredWindowAttributes(hwnd, NULL, 255, LWA_ALPHA);
  EXPECT_FALSE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  RemoveStyle(hwnd, GWL_EXSTYLE, WS_EX_LAYERED);
  EXPECT_TRUE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  // Any complex region should fail, as we only consider simple rectangular
  // windows. In this case, we make the region a triangle.
  POINT point1;
  point1.x = 0;
  point1.y = 0;
  POINT point2;
  point2.x = 50;
  point2.y = 0;
  POINT point3;
  point3.x = 0;
  point3.y = 50;
  POINT points[] = {point1, point2, point3};
  base::win::ScopedRegion complex_region(CreatePolygonRgn(points, 3, WINDING));
  SetWindowRgn(hwnd, complex_region.get(), TRUE);
  EXPECT_FALSE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));

  // A rectangular region should pass.
  base::win::ScopedRegion rectangular_region(CreateRectRgn(200, 200, 200, 200));
  SetWindowRgn(hwnd, rectangular_region.get(), TRUE);
  EXPECT_TRUE(IsWindowVisibleAndFullyOpaque(hwnd, &rect));
}

class MockWindowEvaluator : public WindowEvaluator {
 public:
  // At the end of the test, the window_stack must be empty, otherwise not all
  // the windows were seen.
  ~MockWindowEvaluator() { EXPECT_TRUE(window_stack.empty()); }

  // Tests that EnumWindows goes front to back by creating a stack of aura
  // windows and popping the top window off the stack as its HWND is seen in
  // this callback. If the stack isn't empty at the end, EnumWindows did not see
  // all the windows, or did not see them in the correct order.
  bool EvaluateWindow(bool is_relevant,
                      const gfx::Rect& window_rect_in_pixels,
                      HWND hwnd) override {
    if (window_stack.empty())
      return FALSE;

    HWND top_window_hwnd =
        window_stack.top()->GetHost()->GetAcceleratedWidget();

    if (hwnd == top_window_hwnd)
      window_stack.pop();

    return TRUE;
  }

  void AddToStack(aura::Window* window) { window_stack.push(window); }

 private:
  base::stack<aura::Window*> window_stack;
};

// Tests the functionality of EnumWindows. Specifically:
//  1) EnumWindows enumerates all windows on the desktop.
//  2) EnumWindows enumerates from front to back in the Z-order.
// This needs to be tested because 2) is undocumented behavior. However, this
// behavior has been observed in the community and tested to be true.
// ComputeNativeWindowOcclusionStatus() relies on this assumption.
class EnumWindowsTest : public aura::test::AuraTestBase {
 public:
  EnumWindowsTest() {}

  void TearDown() override {
    window_tree_hosts_.clear();
    aura::test::AuraTestBase::TearDown();
  }

  void CreateAuraWindowWithBounds(const gfx::Rect& bounds) {
    std::unique_ptr<aura::WindowTreeHost> host =
        aura::WindowTreeHost::Create(ui::PlatformWindowInitProperties{bounds});
    host->window()->Show();
    evaluator_.AddToStack(host->window());
    window_tree_hosts_.push_back(std::move(host));
  }

  void TestIterator() {
    // The iterator will validate that the OS returns the full set of windows in
    // the expected order, as encoded by |window_stack| in |evaluator_|
    WindowsDesktopWindowIterator iterator;
    iterator.Iterate(&evaluator_);
  }

 private:
  std::vector<std::unique_ptr<aura::WindowTreeHost>> window_tree_hosts_;

  MockWindowEvaluator evaluator_;

  DISALLOW_COPY_AND_ASSIGN(EnumWindowsTest);
};

TEST_F(EnumWindowsTest, EnumWindowsGoesFrontToBack) {
  CreateAuraWindowWithBounds(gfx::Rect(0, 0, 100, 100));
  CreateAuraWindowWithBounds(gfx::Rect(50, 50, 500, 500));
  CreateAuraWindowWithBounds(gfx::Rect(20, 20, 300, 50));
  CreateAuraWindowWithBounds(gfx::Rect(200, 200, 10, 10));
  CreateAuraWindowWithBounds(gfx::Rect(0, 0, 100, 100));

  TestIterator();
}

}  // namespace aura_extra

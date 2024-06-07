// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_occlusion_tracker.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/window_occlusion_tracker_test_api.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_change_builder.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/gfx/interpolated_transform.h"

namespace aura {

namespace {

constexpr base::TimeDelta kTransitionDuration = base::Seconds(3);

class FakeWindowOcclusionChangeBuilder : public WindowOcclusionChangeBuilder {
 public:
  FakeWindowOcclusionChangeBuilder() = default;
  FakeWindowOcclusionChangeBuilder(const FakeWindowOcclusionChangeBuilder&) =
      delete;
  FakeWindowOcclusionChangeBuilder& operator=(
      const FakeWindowOcclusionChangeBuilder&) = delete;
  ~FakeWindowOcclusionChangeBuilder() override = default;

  // WindowOcclusionChangeBuilder:
  void Add(Window* window,
           Window::OcclusionState occlusion_state,
           SkRegion occluded_region) override {}
};

class MockWindowDelegate : public test::ColorTestWindowDelegate {
 public:
  MockWindowDelegate() : test::ColorTestWindowDelegate(SK_ColorWHITE) {}

  MockWindowDelegate(const MockWindowDelegate&) = delete;
  MockWindowDelegate& operator=(const MockWindowDelegate&) = delete;

  ~MockWindowDelegate() override { EXPECT_FALSE(is_expecting_call()); }

  void set_window(Window* window) { window_ = window; }
  Window* window() { return window_; }

  void SetName(const std::string& name) { window_->SetName(name); }

  void set_expectation(Window::OcclusionState occlusion_state,
                       SkRegion occluded_region = SkRegion()) {
    expected_occlusion_state_ = occlusion_state;
    expected_occluded_region_ = occluded_region;
  }

  bool is_expecting_call() const {
    return expected_occlusion_state_ != Window::OcclusionState::UNKNOWN;
  }

  void OnWindowOcclusionChanged(
      Window::OcclusionState old_occlusion_state,
      Window::OcclusionState new_occlusion_state) override {
    SCOPED_TRACE(window_->GetName());
    ASSERT_TRUE(window_);
    EXPECT_NE(new_occlusion_state, Window::OcclusionState::UNKNOWN);
    EXPECT_EQ(new_occlusion_state, expected_occlusion_state_);
    EXPECT_EQ(window_->occluded_region_in_root(), expected_occluded_region_);
    expected_occlusion_state_ = Window::OcclusionState::UNKNOWN;
    expected_occluded_region_ = SkRegion();
  }

 private:
  Window::OcclusionState expected_occlusion_state_ =
      Window::OcclusionState::UNKNOWN;
  SkRegion expected_occluded_region_ = SkRegion();
  raw_ptr<Window> window_ = nullptr;
};

class WindowOcclusionTrackerTest : public test::AuraTestBase {
 public:
  WindowOcclusionTrackerTest() = default;

  WindowOcclusionTrackerTest(const WindowOcclusionTrackerTest&) = delete;
  WindowOcclusionTrackerTest& operator=(const WindowOcclusionTrackerTest&) =
      delete;

#if BUILDFLAG(IS_WIN)
  void SetUp() override {
    // Native Window Occlusion calculation runs in the background and can
    // interfere with the expectations of these tests, so, disable it.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kCalculateNativeWinOcclusion});
    AuraTestBase::SetUp();
  }
#endif

  Window* CreateTrackedWindow(
      MockWindowDelegate* delegate,
      const gfx::Rect& bounds,
      Window* parent = nullptr,
      bool transparent = false,
      ui::LayerType layer_type = ui::LAYER_TEXTURED,
      WindowOcclusionTracker* secondary_occlusion_tracker = nullptr) {
    Window* window = new Window(delegate);
    delegate->set_window(window);
    window->SetType(client::WINDOW_TYPE_NORMAL);
    window->Init(layer_type);
    if (layer_type == ui::LAYER_SOLID_COLOR)
      window->layer()->SetColor(SK_ColorBLACK);
    window->SetTransparent(transparent);
    window->SetBounds(bounds);
    window->Show();
    parent = parent ? parent : root_window();
    parent->AddChild(window);
    if (secondary_occlusion_tracker) {
      secondary_occlusion_tracker->Track(window);
    } else {
      window->TrackOcclusionState();
    }
    return window;
  }

  Window* CreateUntrackedWindow(const gfx::Rect& bounds,
                                Window* parent = nullptr,
                                ui::LayerType layer_type = ui::LAYER_TEXTURED) {
    if (layer_type == ui::LAYER_TEXTURED) {
      return test::CreateTestWindow(SK_ColorWHITE, 1, bounds,
                                    parent ? parent : root_window());
    }
    DCHECK_EQ(ui::LAYER_SOLID_COLOR, layer_type);
    Window* window = new Window(nullptr);
    window->SetType(client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_SOLID_COLOR);
    window->layer()->SetColor(SK_ColorBLACK);
    window->SetBounds(bounds);
    root_window()->AddChild(window);
    window->Show();
    return window;
  }

  WindowOcclusionTracker& GetOcclusionTracker() {
    return *Env::GetInstance()->GetWindowOcclusionTracker();
  }

  std::unique_ptr<WindowOcclusionTracker> CreateSecondaryOcclusionTracker() {
    auto occlusion_tracker = std::make_unique<WindowOcclusionTracker>();
    // Any secondary trackers should not be mutating the `aura::Window`'s
    // occlusion state. That is the sole responsibility of the primary tracker
    // in `aura::Env`.
    occlusion_tracker->set_occlusion_change_builder_factory(base::BindRepeating(
        []() -> std::unique_ptr<WindowOcclusionChangeBuilder> {
          return std::make_unique<FakeWindowOcclusionChangeBuilder>();
        }));
    return occlusion_tracker;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

SkRegion SkRegionFromSkIRects(std::initializer_list<SkIRect> rects) {
  SkRegion r;
  r.setRects(rects.begin(), rects.size());
  return r;
}

}  // namespace

// Verify that non-overlapping windows have a VISIBLE occlusion state.
// _____  _____
// |    | |    |
// |____| |____|
TEST_F(WindowOcclusionTrackerTest, NonOverlappingWindows) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(15, 0, 10, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(15, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that partially overlapping windows have a VISIBLE occlusion state.
// ______
// |__|  |
// |_____|
TEST_F(WindowOcclusionTrackerTest, PartiallyOverlappingWindow) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeWH(5, 5)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that a window whose bounds are covered by a hidden window is not
// occluded. Also, verify that calling Show() on the hidden window causes
// occlusion states to be recomputed.
//  __....     ... = hidden window
// |__|  .
// .......
TEST_F(WindowOcclusionTrackerTest, HiddenWindowCoversWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. Expect it to be non-occluded and expect window a to be
  // occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 15, 15));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Hide window b. Expect window a to be non-occluded and window b to be
  // occluded.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
  window_b->Hide();
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Show window b. Expect window a to be occluded and window b to be non-
  // occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->Show();
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

class WindowOcclusionTrackerOpacityTest
    : public WindowOcclusionTrackerTest,
      public testing::WithParamInterface<bool> {
 public:
  WindowOcclusionTrackerOpacityTest() = default;
  ~WindowOcclusionTrackerOpacityTest() override = default;
  WindowOcclusionTrackerOpacityTest(const WindowOcclusionTrackerOpacityTest&) =
      delete;
  WindowOcclusionTrackerOpacityTest& operator=(
      const WindowOcclusionTrackerOpacityTest&) = delete;

  // WindowOcclusionTrackerTest:
  void SetUp() override {
    use_solid_color_layer_ = GetParam();
    WindowOcclusionTrackerTest::SetUp();
  }

  void SetOpacity(aura::Window* window, float opacity) {
    if (use_solid_color_layer_)
      window->layer()->SetColor(SkColorSetARGB(255 * opacity, 255, 255, 255));
    else
      window->layer()->SetOpacity(opacity);
  }

  ui::LayerType layer_type() const {
    return use_solid_color_layer_ ? ui::LAYER_SOLID_COLOR : ui::LAYER_TEXTURED;
  }

 private:
  bool use_solid_color_layer_;
};

// Verify that a window whose bounds are covered by a semi-transparent window is
// not occluded. Also, verify that that when the opacity of a window changes,
// occlusion states are updated.
//  __....     ... = semi-transparent window
// |__|  .
// .......
TEST_P(WindowOcclusionTrackerOpacityTest, SemiTransparentWindowCoversWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. Expect it to be non-occluded and expect window a to be
  // occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 15, 15),
                                         nullptr, false, layer_type());
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Change the opacity of window b to 0.5f. Expect both windows to be non-
  // occluded.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  SetOpacity(window_b, 0.5f);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Change the opacity of window b back to 1.0f. Expect window a to be
  // occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  SetOpacity(window_b, 1.f);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Same as previous test, but the occlusion state of the semi-transparent is not
// tracked.
TEST_P(WindowOcclusionTrackerOpacityTest,
       SemiTransparentUntrackedWindowCoversWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create untracked window b. Expect window a to be occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_b =
      CreateUntrackedWindow(gfx::Rect(0, 0, 15, 15), nullptr, layer_type());
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Change the opacity of window b to 0.5f. Expect both windows to be non-
  // occluded.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  SetOpacity(window_b, 0.5f);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Change the opacity of window b back to 1.0f. Expect window a to be
  // occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  SetOpacity(window_b, 1.0f);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that one window whose bounds are covered by a set of two opaque
// windows is occluded.
//  ______
// |  |  |  <-- these two windows cover another window
// |__|__|
TEST_F(WindowOcclusionTrackerTest, TwoWindowsOccludeOneWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b with bounds that partially cover window a. Expect both
  // windows to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeWH(5, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 5, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c with bounds that cover the portion of window a that isn't
  // already covered by window b. Expect window a to be occluded and window a/b
  // to be non-occluded.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(5, 0, 5, 10)));
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(5, 0, 5, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that a window and its child that are covered by a sibling are
// occluded.
TEST_F(WindowOcclusionTrackerTest, SiblingOccludesWindowAndChild) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 20, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b, with bounds that occlude half of its parent window a.
  // Expect it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 20), window_a);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c, with bounds that occlude window a and window b. Expect it
  // to be non-occluded, and window a and b to be occluded.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 20, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that a window with one half covered by a child and the other half
// covered by a sibling is non-occluded.
TEST_F(WindowOcclusionTrackerTest, ChildAndSiblingOccludeOneWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 20, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b, with bounds that occlude half of its parent window a.
  // Expect it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 20), window_a);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c, with bounds that occlude the other half of window a.
  // Expect it to be non-occluded and expect window a to remain non-occluded.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(10, 0, 10, 20)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(10, 0, 10, 20)));
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(10, 0, 10, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that a window covered by 2 non-occluded children is non-occluded.
TEST_F(WindowOcclusionTrackerTest, ChildrenOccludeOneWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 20, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b, with bounds that cover half of its parent window a. Expect
  // it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 20), window_a);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window c, with bounds that cover the other half of its parent window
  // a. Expect it to be non-occluded. Expect window a to remain non-occluded.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(10, 0, 10, 20)));
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(10, 0, 10, 20), window_a);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that when the bounds of a child window covers the bounds of a parent
// window but is itself visible, the parent window is visible.
TEST_F(WindowOcclusionTrackerTest, ChildDoesNotOccludeParent) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b with window a as parent. The bounds of window b fully cover
  // window a. Expect both windows to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b =
      CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10), window_a);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c whose bounds don't overlap existing windows.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(15, 0, 10, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(15, 0, 10, 10)));
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(15, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Change the parent of window b from window a to window c. Expect all windows
  // to remain non-occluded.
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_c->AddChild(window_b);
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that when the stacking order of windows change, occlusion states are
// updated.
TEST_F(WindowOcclusionTrackerTest, StackingChanged) {
  // Create three windows that have the same bounds. Expect window on top of the
  // stack to be non-occluded and other windows to be occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Move window a on top of the stack. Expect it to be non-occluded and expect
  // window c to be occluded.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  root_window()->StackChildAtTop(window_a);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that when the stacking order of two transparent window changes, the
// occlusion states of their children is updated. The goal of this test is to
// ensure that the fact that the windows whose stacking order change are
// transparent doesn't prevent occlusion states from being recomputed.
TEST_F(WindowOcclusionTrackerTest, TransparentParentStackingChanged) {
  // Create window a which is transparent. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10),
                                         root_window(), true);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which has the same bounds as its parent window a. Expect it
  // to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10), window_a);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c which is transparent and has the same bounds as window a
  // and window b. Expect it to be non-occluded.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 10, 10),
                                         root_window(), true);
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Create window d which has the same bounds as its parent window c. Expect
  // window d to be non-occluded and window a and b to be occluded.
  MockWindowDelegate* delegate_d = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_d->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_d, gfx::Rect(0, 0, 10, 10), window_c);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_d->is_expecting_call());

  // Move window a on top of the stack. Expect window a and b to be non-occluded
  // and window c and d to be occluded.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_d->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  root_window()->StackChildAtTop(window_a);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
  EXPECT_FALSE(delegate_d->is_expecting_call());
}

// Verify that when StackChildAtTop() is called on a window whose occlusion
// state is not tracked, the occlusion state of tracked siblings is updated.
TEST_F(WindowOcclusionTrackerTest, UntrackedWindowStackingChanged) {
  Window* window_a = CreateUntrackedWindow(gfx::Rect(0, 0, 5, 5));

  // Create window b. Expect it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Stack window a on top of window b. Expect window b to be occluded.
  delegate_b->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  root_window()->StackChildAtTop(window_a);
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that occlusion states are updated when the bounds of a window change.
TEST_F(WindowOcclusionTrackerTest, BoundsChanged) {
  // Create two non-overlapping windows. Expect them to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 10, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Move window b on top of window a. Expect window a to be occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  window_b->SetBounds(window_a->bounds());
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that when the bounds of a window are animated, occlusion states are
// updated at the beginning and at the end of the animation, but not during the
// animation. At the beginning of the animation, the window animated window
// should be considered non-occluded and should not occlude other windows. The
// animated window starts occluded.
TEST_F(WindowOcclusionTrackerTest, OccludedWindowBoundsAnimated) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create 3 windows. Window a is unoccluded. Window c occludes window b.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 10, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  window_b->layer()->SetAnimator(test_controller.animator());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Start animating the bounds of window b so that it moves on top of window a.
  // Window b should be non-occluded when the animation starts.
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->SetBounds(window_a->bounds());
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Window a should remain non-occluded during the animation.
  test_controller.Step(kTransitionDuration / 3);

  // Window b should occlude window a at the end of the animation.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  // Window b should have window c in its potential occlusion region.
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 10, 10)));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  window_b->layer()->SetAnimator(nullptr);
}

// Same as the previous test, but the animated window starts non-occluded.
TEST_F(WindowOcclusionTrackerTest, NonOccludedWindowBoundsAnimated) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create 3 windows. Window a is unoccluded. Window c occludes window b.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 10, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
  window_c->layer()->SetAnimator(test_controller.animator());

  // Start animating the bounds of window c so that it moves on top of window a.
  // Window b should be non-occluded when the animation starts.
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_c->SetBounds(window_a->bounds());
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Window a should remain non-occluded during the animation.
  test_controller.Step(kTransitionDuration / 3);

  // Window c should occlude window a at the end of the animation.
  // Window b should have a potentially occluded region including window c.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 0, 10, 10)));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  window_c->layer()->SetAnimator(nullptr);
}

// Verify that occlusion states are updated when the bounds of a transparent
// window with opaque children change.
TEST_F(WindowOcclusionTrackerTest, TransparentParentBoundsChanged) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which doesn't overlap window a and is transparent. Expect
  // it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10),
                                         root_window(), true);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c which has window b as parent and doesn't occlude any
  // window.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 5, 5)));
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 5, 5), window_b);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Move window b so that window c occludes window a. Expect window a to be
  // occluded and other windows to be non-occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  window_b->SetBounds(gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that occlusion states are updated when the bounds of a window whose
// occlusion state is not tracked change.
TEST_F(WindowOcclusionTrackerTest, UntrackedWindowBoundsChanged) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. It should not occlude window a.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 5, 5)));
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Move window b on top of window a. Expect window a to be occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  window_b->SetBounds(window_a->bounds());
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that occlusion states are updated when the transform of a window
// changes.
TEST_F(WindowOcclusionTrackerTest, TransformChanged) {
  // Create two non-overlapping windows. Expect them to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 5, 5)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Scale and translate window b so that it covers window a. Expect window a to
  // be occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  gfx::Transform transform;
  transform.Translate(0.0f, -10.0f);
  transform.Scale(2.0f, 2.0f);
  window_b->SetTransform(transform);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that when the transform of a window is animated, occlusion states are
// updated at the beginning and at the end of the animation, but not during the
// animation. At the beginning of the animation, the window animated window
// should be considered non-occluded and should not occlude other windows. The
// animated window starts occluded.
TEST_F(WindowOcclusionTrackerTest, OccludedWindowTransformAnimated) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create 3 windows. Window a is unoccluded. Window c occludes window b.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 5, 5)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  window_b->layer()->SetAnimator(test_controller.animator());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Start animating the transform of window b so that it moves on top of window
  // a. Window b should be non-occluded when the animation starts.
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  auto transform = std::make_unique<ui::InterpolatedScale>(
      gfx::Point3F(1, 1, 1), gfx::Point3F(2.0f, 2.0f, 1));
  transform->SetChild(std::make_unique<ui::InterpolatedTranslation>(
      gfx::PointF(), gfx::PointF(-0.0f, -10.0f)));
  test_controller.animator()->StartAnimation(new ui::LayerAnimationSequence(
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(transform), kTransitionDuration)));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Window a should remain non-occluded during the animation.
  test_controller.Step(kTransitionDuration / 3);

  // Window b should occlude window a at the end of the animation.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  // Window b should see window c as part of the potential occlusion region.
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 5, 5)));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  window_b->layer()->SetAnimator(nullptr);
}

// Same as the previous test, but the animated window starts non-occluded.
TEST_F(WindowOcclusionTrackerTest, NonOccludedWindowTransformAnimated) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create 3 windows. Window a is unoccluded. Window c occludes window b.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 20, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 20, 10, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 20, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(0, 20, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
  window_c->layer()->SetAnimator(test_controller.animator());

  // Start animating the bounds of window c so that it moves on top of window a.
  // Window b should be non-occluded when the animation starts.
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  auto transform = std::make_unique<ui::InterpolatedScale>(
      gfx::Point3F(1, 1, 1), gfx::Point3F(2.0f, 2.0f, 1));
  transform->SetChild(std::make_unique<ui::InterpolatedTranslation>(
      gfx::PointF(), gfx::PointF(-0.0f, -20.0f)));
  test_controller.animator()->StartAnimation(new ui::LayerAnimationSequence(
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(transform), kTransitionDuration)));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Window a should remain non-occluded during the animation.
  test_controller.Step(kTransitionDuration / 3);

  // Window c should occlude window a at the end of the animation.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  // Window b should now see window c in the potential occlusion region.
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeWH(20, 20)));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  window_c->layer()->SetAnimator(nullptr);
}

// Verify that occlusion states are updated when the transform of a transparent
// window with opaque children change.
TEST_F(WindowOcclusionTrackerTest, TransparentParentTransformChanged) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which doesn't overlap window a and is transparent. Expect
  // it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10),
                                         root_window(), true);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c which has window b as parent and doesn't occlude any
  // window.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 5, 5)));
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 5, 5), window_b);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Scale and translate window b so that window c occludes window a. Expect
  // window a to be occluded and other windows to be non-occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  gfx::Transform transform;
  transform.Translate(0.0f, -10.0f);
  transform.Scale(2.0f, 2.0f);
  window_b->SetTransform(transform);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that occlusion states are updated when the transform of a window whose
// occlusion state is not tracked changes.
TEST_F(WindowOcclusionTrackerTest, UntrackedWindowTransformChanged) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. It should not occlude window a.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 5, 5)));
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Scale and translate window b so that it occludes window a. Expect window a
  // to be occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  gfx::Transform transform;
  transform.Translate(0.0f, -10.0f);
  transform.Scale(2.0f, 2.0f);
  window_b->SetTransform(transform);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that deleting an untracked window which covers a tracked window causes
// the tracked window to be non-occluded.
TEST_F(WindowOcclusionTrackerTest, DeleteUntrackedWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which occludes window a.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Delete window b. Expect a to be non-occluded.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delete window_b;
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that removing an untracked window which covers a tracked window causes
// the tracked window to be non-occluded.
TEST_F(WindowOcclusionTrackerTest, RemoveUntrackedWindow) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which occludes window a.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Delete window b. Expect a to be non-occluded.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  root_window()->RemoveChild(window_b);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  delete window_b;
}

// Verify that when a tracked window is removed and re-added to a root,
// occlusion states are still tracked.
TEST_F(WindowOcclusionTrackerTest, RemoveAndAddTrackedToRoot) {
  Window* window_a = CreateUntrackedWindow(gfx::Rect(0, 0, 1, 1));
  CreateUntrackedWindow(gfx::Rect(0, 0, 10, 10), window_a);

  // Create window b. Expect it to be non-occluded.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Remove window c from its root.
  root_window()->RemoveChild(window_c);

  // Add window c back under its root.
  root_window()->AddChild(window_c);

  // Create untracked window d which covers window a. Expect window a to be
  // occluded.
  delegate_c->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_d = CreateUntrackedWindow(gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Move window d so that it doesn't cover window c.
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 5, 5)));
  window_d->SetBounds(gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Stack window a on top of window c. Expect window c to be non-occluded. This
  // won't work if WindowOcclusionTracked didn't register as an observer of
  // window a when window c was made a child of root_window().
  delegate_c->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  root_window()->StackChildAtTop(window_a);
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

namespace {

class ResizeWindowObserver : public WindowObserver {
 public:
  ResizeWindowObserver(Window* window_to_resize)
      : window_to_resize_(window_to_resize) {}

  ResizeWindowObserver(const ResizeWindowObserver&) = delete;
  ResizeWindowObserver& operator=(const ResizeWindowObserver&) = delete;

  void OnWindowBoundsChanged(Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    window_to_resize_->SetBounds(old_bounds);
  }

 private:
  const raw_ptr<Window> window_to_resize_;
};

}  // namespace

// Verify that when the bounds of a child window are updated in response to the
// bounds of a parent window being updated, occlusion states are updated once.
TEST_F(WindowOcclusionTrackerTest, ResizeChildFromObserver) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. Expect it to be non-occluded and to occlude window a.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create window c, which is a child of window b. Expect it to be non-
  // occluded.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_c =
      CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 5, 5), window_b);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create an observer that will resize window c when window b is resized.
  ResizeWindowObserver resize_window_observer(window_c);
  window_b->AddObserver(&resize_window_observer);

  // Resize window b. Expect window c to be resized so that window a stays
  // occluded. Window a should not temporarily be non-occluded.
  window_b->SetBounds(gfx::Rect(0, 0, 5, 5));

  window_b->RemoveObserver(&resize_window_observer);
}

// Verify that the bounds of windows are changed multiple times within the scope
// of a ScopedPause, occlusion states are updated once at the end of the scope.
TEST_F(WindowOcclusionTrackerTest, ScopedPause) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b which doesn't overlap window a. Expect it to be non
  // occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 5, 5)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Change bounds multiple times. At the end of the scope, expect window a to
  // be occluded.
  {
    WindowOcclusionTracker::ScopedPause pause_occlusion_tracking;
    window_b->SetBounds(window_a->bounds());
    window_a->SetBounds(gfx::Rect(0, 10, 5, 5));
    window_b->SetBounds(window_a->bounds());

    delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  }
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Same as the previous test, but with nested ScopedPause.
TEST_F(WindowOcclusionTrackerTest, NestedScopedPause) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  // Create window b which doesn't overlap window a. Expect it to be non-
  // occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 5, 5)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Change bounds multiple times. At the end of the scope, expect window a to
  // be occluded.
  {
    WindowOcclusionTracker::ScopedPause pause_occlusion_tracking_a;

    {
      WindowOcclusionTracker::ScopedPause pause_occlusion_tracking_b;
      window_b->SetBounds(window_a->bounds());
    }
    {
      WindowOcclusionTracker::ScopedPause pause_occlusion_tracking_c;
      window_a->SetBounds(gfx::Rect(0, 10, 5, 5));
    }
    {
      WindowOcclusionTracker::ScopedPause pause_occlusion_tracking_d;
      window_b->SetBounds(window_a->bounds());
    }

    delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  }
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that bounds are computed correctly when a hierarchy of windows have
// transforms.
TEST_F(WindowOcclusionTrackerTest, HierarchyOfTransforms) {
  gfx::Transform scale_2x_transform;
  scale_2x_transform.Scale(2.0f, 2.0f);

  // Effective bounds: x = 2, y = 2, height = 10, width = 10
  Window* window_a = CreateUntrackedWindow(gfx::Rect(2, 2, 5, 5));
  window_a->SetTransform(scale_2x_transform);

  // Effective bounds: x = 4, y = 4, height = 4, width = 4
  Window* window_b = CreateUntrackedWindow(gfx::Rect(1, 1, 2, 2), window_a);

  // Effective bounds: x = 34, y = 36, height = 8, width = 10
  CreateUntrackedWindow(gfx::Rect(15, 16, 4, 5), window_b);
  MockWindowDelegate* delegate_d = new MockWindowDelegate();
  delegate_d->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_d = CreateTrackedWindow(delegate_d, gfx::Rect(34, 36, 8, 10));
  EXPECT_FALSE(delegate_d->is_expecting_call());

  delegate_d->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  root_window()->StackChildAtBottom(window_d);
  EXPECT_FALSE(delegate_d->is_expecting_call());

  SkRegion occluded_area = SkRegionFromSkIRects(
      {SkIRect::MakeXYWH(2, 2, 10, 10), SkIRect::MakeXYWH(4, 4, 4, 4),
       SkIRect::MakeXYWH(34, 36, 8, 10)});
  delegate_d->set_expectation(Window::OcclusionState::VISIBLE, occluded_area);
  window_d->SetBounds(gfx::Rect(35, 36, 8, 10));
  EXPECT_FALSE(delegate_d->is_expecting_call());

  // |window_d| should remain non-occluded with the following bounds changes.
  window_d->SetBounds(gfx::Rect(33, 36, 8, 10));
  window_d->SetBounds(gfx::Rect(34, 37, 8, 10));
  window_d->SetBounds(gfx::Rect(34, 35, 8, 10));
}

// Verify that clipping is taken into account when computing occlusion.
TEST_F(WindowOcclusionTrackerTest, Clipping) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. Expect it to be non-occluded.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeWH(5, 5)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  window_b->layer()->SetMasksToBounds(true);
  // Create window c which has window b as parent. Don't expect it to occlude
  // window a since its bounds are clipped by window b.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 10, 10), window_b);
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that the DCHECK(!WindowIsAnimated(window)) in
// WindowOcclusionTracker::OnWindowDestroyed() doesn't fire if a window is
// destroyed with an incomplete animation (~Window should complete the animation
// and the window should be removed from |animated_windows_| before
// OnWindowDestroyed() is called).
TEST_F(WindowOcclusionTrackerTest, DestroyWindowWithPendingAnimation) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  MockWindowDelegate* delegate = new MockWindowDelegate();
  delegate->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window = CreateTrackedWindow(delegate, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate->is_expecting_call());
  window->layer()->SetAnimator(test_controller.animator());

  // Start animating the bounds of window.
  window->SetBounds(gfx::Rect(10, 10, 5, 5));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_TRUE(test_controller.animator()->IsAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS));

  // Destroy the window. Expect no DCHECK failure.
  delete window;
}

// Verify that `WindowOcclusionTracker` can be destroyed safely with a pending
// animation. This mostly applies to secondary `WindowOcclusionTracker`s,
// not the long-lived one in `aura::Env`.
TEST_F(WindowOcclusionTrackerTest,
       DestroyOcclusionTrackerWithPendingAnimation) {
  auto occlusion_tracker = CreateSecondaryOcclusionTracker();
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  Window* window = CreateTrackedWindow(
      new MockWindowDelegate, gfx::Rect(0, 0, 10, 10), /*parent=*/nullptr,
      /*transparent=*/false, ui::LAYER_TEXTURED, occlusion_tracker.get());
  window->layer()->SetAnimator(test_controller.animator());

  // Start animating the bounds of window.
  window->SetBounds(gfx::Rect(10, 10, 5, 5));
  test_controller.Step(kTransitionDuration / 3);
  ASSERT_TRUE(test_controller.animator()->IsAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS));
  // There's no explicit test expectation here other not crashing on shutdown.
  occlusion_tracker.reset();

  // Start animating the bounds of window again. Ensures more animations can
  // be started without crashes.
  test_controller.animator()->AbortAllAnimations();
  window->SetBounds(gfx::Rect(20, 20, 10, 10));
  test_controller.Step(kTransitionDuration / 2);
}

// Verify that an animated window stops being considered as animated when its
// layer is recreated.
TEST_F(WindowOcclusionTrackerTest, RecreateLayerOfAnimatedWindow) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create 2 windows. Window b occludes window a.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(2, 2, 1, 1));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  window_a->layer()->SetAnimator(test_controller.animator());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Start animating the bounds of window a. Window a should be non-occluded
  // when the animation starts.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_a->SetBounds(gfx::Rect(6, 6, 1, 1));
  test_controller.Step(kTransitionDuration / 2);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Recreate the layer of window b. Expect this to behave the same as if the
  // animation was abandoned. Occlusion region should be half way between the
  // animation bounds.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  std::unique_ptr<ui::Layer> old_layer = window_a->RecreateLayer();
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_a->layer()->SetAnimator(nullptr);
}

namespace {

class ObserverChangingWindowBounds : public WindowObserver {
 public:
  ObserverChangingWindowBounds() = default;

  ObserverChangingWindowBounds(const ObserverChangingWindowBounds&) = delete;
  ObserverChangingWindowBounds& operator=(const ObserverChangingWindowBounds&) =
      delete;

  // WindowObserver:
  void OnWindowParentChanged(Window* window, Window* parent) override {
    window->SetBounds(gfx::Rect(1, 2, 3, 4));
  }
};

}  // namespace

// Verify that no crash occurs if a tracked window is modified by an observer
// after it has been added to a new root but before WindowOcclusionTracker has
// been notified.
TEST_F(WindowOcclusionTrackerTest, ChangeTrackedWindowBeforeObserveAddToRoot) {
  // Create a window. Expect it to be non-occluded.
  MockWindowDelegate* delegate = new MockWindowDelegate();
  delegate->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window = CreateTrackedWindow(delegate, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate->is_expecting_call());

  // Remove the window from its root.
  root_window()->RemoveChild(window);

  // Add an observer that changes the bounds of |window| when it gets a new
  // parent.
  ObserverChangingWindowBounds observer;
  window->AddObserver(&observer);

  // Re-add the window to its root. Expect no crash when |observer| changes the
  // bounds.
  root_window()->AddChild(window);

  window->RemoveObserver(&observer);
}

namespace {

class ObserverDestroyingWindowOnAnimationEnded
    : public ui::LayerAnimationObserver {
 public:
  ObserverDestroyingWindowOnAnimationEnded(Window* window) : window_(window) {}

  ObserverDestroyingWindowOnAnimationEnded(
      const ObserverDestroyingWindowOnAnimationEnded&) = delete;
  ObserverDestroyingWindowOnAnimationEnded& operator=(
      const ObserverDestroyingWindowOnAnimationEnded&) = delete;

  ~ObserverDestroyingWindowOnAnimationEnded() override {
    EXPECT_FALSE(window_);
  }

  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    EXPECT_TRUE(window_);
    delete window_;
    window_ = nullptr;
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

 private:
  raw_ptr<Window, DanglingUntriaged> window_;
};

}  // namespace

// Verify that no crash occurs if a LayerAnimationObserver destroys a tracked
// window before WindowOcclusionTracker is notified that the animation ended.
TEST_P(WindowOcclusionTrackerOpacityTest,
       DestroyTrackedWindowFromLayerAnimationObserver) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create a window. Expect it to be non-occluded.
  MockWindowDelegate* delegate = new MockWindowDelegate();
  delegate->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window = CreateTrackedWindow(delegate, gfx::Rect(0, 0, 10, 10),
                                       nullptr, false, layer_type());
  EXPECT_FALSE(delegate->is_expecting_call());
  window->layer()->SetAnimator(test_controller.animator());

  // Add a LayerAnimationObserver that destroys the window when an animation
  // ends.
  ObserverDestroyingWindowOnAnimationEnded observer(window);
  window->layer()->GetAnimator()->AddObserver(&observer);

  // Start animating the opacity of the window.
  SetOpacity(window, 0.5f);

  // Complete the animation. Expect no crash.
  window->layer()->GetAnimator()->StopAnimating();
}

// Verify that no crash occurs if an animation completes on a non-tracked
// window's layer after the window has been removed from a root with a tracked
// window and deleted.
TEST_P(WindowOcclusionTrackerOpacityTest,
       DeleteNonTrackedAnimatedWindowRemovedFromTrackedRoot) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create a tracked window. Expect it to be non-occluded.
  MockWindowDelegate* delegate = new MockWindowDelegate();
  delegate->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate->is_expecting_call());

  // Create a non-tracked window and add an observer that deletes it when its
  // stops being animated.
  delegate->set_expectation(Window::OcclusionState::VISIBLE,
                            SkRegion(SkIRect::MakeXYWH(10, 0, 10, 10)));
  Window* window =
      CreateUntrackedWindow(gfx::Rect(10, 0, 10, 10), nullptr, layer_type());
  EXPECT_FALSE(delegate->is_expecting_call());

  window->layer()->SetAnimator(test_controller.animator());
  ObserverDestroyingWindowOnAnimationEnded observer(window);
  window->layer()->GetAnimator()->AddObserver(&observer);

  // Animate the window. WindowOcclusionTracker should add itself as an observer
  // of its LayerAnimator (after |observer|). Upon beginning animation, the
  // window should no longer affect the occluded region.
  delegate->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  SetOpacity(window, 0.1);
  // Drive the animation so that the color's alpha value changes.
  test_controller.Step(kTransitionDuration / 2);
  EXPECT_FALSE(delegate->is_expecting_call());

  // Remove the non-tracked window from its root. WindowOcclusionTracker should
  // remove the window from its list of animated windows and stop observing it
  // and its LayerAnimator.
  root_window()->RemoveChild(window);

  // Complete animations on the window. |observer| will delete the window when
  // it is notified that animations are complete. Expect that
  // WindowOcclusionTracker will not try to access |window| after that (if it
  // does, the test will crash).
  window->layer()->GetAnimator()->StopAnimating();
}

TEST_P(WindowOcclusionTrackerOpacityTest,
       OpacityAnimationShouldNotOccludeWindow) {
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Create a tracked window. Expect it to be non-occluded.
  MockWindowDelegate* delegate = new MockWindowDelegate();
  delegate->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  auto* foo = CreateTrackedWindow(delegate, gfx::Rect(0, 0, 10, 10));
  foo->SetName("A");
  EXPECT_FALSE(delegate->is_expecting_call());

  // Create a non-tracked window which occludes the tracked window.
  delegate->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window =
      CreateUntrackedWindow(gfx::Rect(0, 0, 10, 10), nullptr, layer_type());
  window->SetName("B");
  EXPECT_FALSE(delegate->is_expecting_call());

  // Set the opacity to make the tracked window VISIBLE.
  delegate->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  SetOpacity(window, 0.1);
  EXPECT_FALSE(delegate->is_expecting_call());

  // Animate the window. WindowOcclusionTracker should add itself as an observer
  // of its LayerAnimator (after |observer|). Upon beginning animation, the
  // window should no longer affect the occluded region.
  window->layer()->SetAnimator(test_controller.animator());
  delegate->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  SetOpacity(window, 1.0);
  EXPECT_TRUE(delegate->is_expecting_call());
  // Drive the animation so that the color's alpha value changes.
  test_controller.Step(kTransitionDuration / 2);
  EXPECT_TRUE(delegate->is_expecting_call());
  window->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(delegate->is_expecting_call());
  window->layer()->SetAnimator(nullptr);
}

namespace {

class WindowDelegateHidingWindowIfOccluded : public MockWindowDelegate {
 public:
  explicit WindowDelegateHidingWindowIfOccluded(Window* other_window)
      : other_window_(other_window) {}

  WindowDelegateHidingWindowIfOccluded(
      const WindowDelegateHidingWindowIfOccluded&) = delete;
  WindowDelegateHidingWindowIfOccluded& operator=(
      const WindowDelegateHidingWindowIfOccluded&) = delete;

  // MockWindowDelegate:
  void OnWindowOcclusionChanged(
      Window::OcclusionState old_occlusion_state,
      Window::OcclusionState new_occlusion_state) override {
    MockWindowDelegate::OnWindowOcclusionChanged(old_occlusion_state,
                                                 new_occlusion_state);
    if (new_occlusion_state == Window::OcclusionState::HIDDEN) {
      other_window_->Hide();
    }
  }

 private:
  raw_ptr<Window, DanglingUntriaged> other_window_;
};

class WindowDelegateWithQueuedExpectation : public MockWindowDelegate {
 public:
  WindowDelegateWithQueuedExpectation() = default;

  WindowDelegateWithQueuedExpectation(
      const WindowDelegateWithQueuedExpectation&) = delete;
  WindowDelegateWithQueuedExpectation& operator=(
      const WindowDelegateWithQueuedExpectation&) = delete;

  void set_queued_expectation(Window::OcclusionState occlusion_state,
                              const SkRegion& occluded_region) {
    queued_expected_occlusion_state_ = occlusion_state;
    queued_expected_occluded_region_ = occluded_region;
  }

  // MockWindowDelegate:
  void OnWindowOcclusionChanged(
      Window::OcclusionState old_occlusion_state,
      Window::OcclusionState new_occlusion_state) override {
    MockWindowDelegate::OnWindowOcclusionChanged(old_occlusion_state,
                                                 new_occlusion_state);
    if (queued_expected_occlusion_state_ != Window::OcclusionState::UNKNOWN) {
      set_expectation(queued_expected_occlusion_state_,
                      queued_expected_occluded_region_);
      queued_expected_occlusion_state_ = Window::OcclusionState::UNKNOWN;
      queued_expected_occluded_region_ = SkRegion();
    }
  }

 private:
  Window::OcclusionState queued_expected_occlusion_state_ =
      Window::OcclusionState::UNKNOWN;
  SkRegion queued_expected_occluded_region_ = SkRegion();
};

}  // namespace

// Verify that a window delegate can change the visibility of another window
// when it is notified that its occlusion changed.
TEST_F(WindowOcclusionTrackerTest, HideFromOnWindowOcclusionChanged) {
  // Create a tracked window. Expect it to be visible.
  WindowDelegateWithQueuedExpectation* delegate_a =
      new WindowDelegateWithQueuedExpectation();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create a tracked window. Expect it to be visible.
  MockWindowDelegate* delegate_b =
      new WindowDelegateHidingWindowIfOccluded(window_a);
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(10, 0, 10, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(10, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Hide the tracked window. It should be able to hide |window_a|. Before
  // |window_a| is hidden, it will notice that the occlusion region has changed
  // now that |window_b| is hidden. Then, it will be hidden by |window_b|.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delegate_a->set_queued_expectation(Window::OcclusionState::HIDDEN,
                                     SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
  window_b->Hide();

  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(window_a->IsVisible());
  EXPECT_FALSE(window_b->IsVisible());
}

namespace {

class WindowDelegateDeletingWindow : public MockWindowDelegate {
 public:
  WindowDelegateDeletingWindow() = default;

  WindowDelegateDeletingWindow(const WindowDelegateDeletingWindow&) = delete;
  WindowDelegateDeletingWindow& operator=(const WindowDelegateDeletingWindow&) =
      delete;

  void set_other_window(Window* other_window) { other_window_ = other_window; }

  // MockWindowDelegate:
  void OnWindowOcclusionChanged(
      Window::OcclusionState old_occlusion_state,
      Window::OcclusionState new_occlusion_state) override {
    MockWindowDelegate::OnWindowOcclusionChanged(old_occlusion_state,
                                                 new_occlusion_state);
    if (new_occlusion_state == Window::OcclusionState::OCCLUDED) {
      delete other_window_;
      other_window_ = nullptr;
    }
  }

 private:
  raw_ptr<Window, DanglingUntriaged> other_window_ = nullptr;
};

}  // namespace

// Verify that a window can delete a window that is on top of it when it is
// notified that its occlusion changed (a crash would occur if
// WindowOcclusionTracker accessed that window after it was deleted).
TEST_F(WindowOcclusionTrackerTest, DeleteFromOnWindowOcclusionChanged) {
  // Create a tracked window. Expect it to be visible.
  WindowDelegateDeletingWindow* delegate_a = new WindowDelegateDeletingWindow();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create a tracked window. Expect it to be visible.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(10, 0, 10, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(10, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create a tracked window. Expect it to be visible.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_a->set_expectation(
      Window::OcclusionState::VISIBLE,
      SkRegionFromSkIRects({SkIRect::MakeXYWH(10, 0, 10, 10),
                            SkIRect::MakeXYWH(20, 0, 10, 10)}));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(20, 0, 10, 10)));
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(20, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // |window_c| will be deleted when |window_a| is occluded.
  delegate_a->set_other_window(window_c);

  // Move |window_b| on top of |window_a|.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->SetBounds(window_a->bounds());
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

namespace {

class WindowDelegateChangingWindowVisibility : public MockWindowDelegate {
 public:
  WindowDelegateChangingWindowVisibility() = default;

  WindowDelegateChangingWindowVisibility(
      const WindowDelegateChangingWindowVisibility&) = delete;
  WindowDelegateChangingWindowVisibility& operator=(
      const WindowDelegateChangingWindowVisibility&) = delete;

  void set_window_to_update(Window* window) { window_to_update_ = window; }

  // MockWindowDelegate:
  void OnWindowOcclusionChanged(
      Window::OcclusionState old_occlusion_state,
      Window::OcclusionState new_occlusion_state) override {
    MockWindowDelegate::OnWindowOcclusionChanged(old_occlusion_state,
                                                 new_occlusion_state);
    if (!window_to_update_)
      return;

    ++num_occlusion_change_;

    if (window_to_update_->IsVisible()) {
      window_to_update_->Hide();
      if (num_occlusion_change_ <= 3)
        set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
    } else {
      window_to_update_->Show();
      set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
    }
  }

 private:
  raw_ptr<Window> window_to_update_ = nullptr;
  int num_occlusion_change_ = 0;
};

}  // namespace

// Verify that if a window changes its visibility every time it is notified that
// its occlusion state changed, a DCHECK occurs.
TEST_F(WindowOcclusionTrackerTest, OcclusionStatesDontBecomeStable) {
  test::WindowOcclusionTrackerTestApi test_api(
      Env::GetInstance()->GetWindowOcclusionTracker());

  // Create 2 superposed tracked windows.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Create a hidden tracked window.
  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(10, 0, 10, 10)));
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_c = CreateTrackedWindow(delegate_c, gfx::Rect(10, 0, 10, 10));
  EXPECT_FALSE(delegate_c->is_expecting_call());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
  window_c->Hide();
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Create a tracked window. Expect it to be non-occluded.
  auto* delegate_d = new WindowDelegateChangingWindowVisibility();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(20, 0, 10, 10)));
  delegate_d->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_d = CreateTrackedWindow(delegate_d, gfx::Rect(20, 0, 10, 10));
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_d->is_expecting_call());

  // Store a pointer to |window_d| in |delegate_d|. This will cause a call to
  // Show()/Hide() every time |delegate_d| is notified of an occlusion change.
  delegate_d->set_window_to_update(window_d);

  // Hide |window_d|. This will cause occlusion to be recomputed multiple times.
  // Once the maximum number of times that occlusion can be recomputed is
  // reached, the occlusion state of all IsVisible() windows should be set to
  // VISIBLE.
  EXPECT_DCHECK_DEATH({
    delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                                SkRegion(SkIRect::MakeXYWH(20, 0, 10, 10)));
    delegate_d->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
    window_d->Hide();
  });
}

// Verify that the occlusion states are correctly updated when a branch of the
// tree is hidden.
TEST_F(WindowOcclusionTrackerTest, HideTreeBranch) {
  // Create a branch of 3 tracked windows. Expect them to be visible.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b =
      CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10), window_a);
  EXPECT_FALSE(delegate_b->is_expecting_call());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 20, 10, 10), window_b);
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Hide |window_b| (and hence |window_c|). Expect |window_b| and |window_c| to
  // be hidden.
  delegate_b->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
  window_b->Hide();
  EXPECT_FALSE(delegate_b->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());
}

// Verify that a window covered by a shaped window isn't considered occluded.
TEST_F(WindowOcclusionTrackerTest, WindowWithAlphaShape) {
  // Create 2 superposed tracked windows.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Set a shape for the top window. The window underneath should no longer be
  // occluded.
  auto shape = std::make_unique<ui::Layer::ShapeRects>();
  shape->emplace_back(0, 0, 5, 5);
  // Shaped windows are not considered opaque, so the occluded region is empty.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->layer()->SetAlphaShape(std::move(shape));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Clear the shape for the top window. The window underneath should be
  // occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  window_b->layer()->SetAlphaShape(nullptr);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that a window covered by a window whose parent has an alpha shape
// isn't considered occluded.
TEST_F(WindowOcclusionTrackerTest, WindowWithParentAlphaShape) {
  // Create a child and parent that cover another window.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 20, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeWH(10, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  MockWindowDelegate* delegate_c = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  delegate_c->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_c, gfx::Rect(0, 0, 20, 20), window_b);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_c->is_expecting_call());

  // Set a shape for |window_b|. |window_a| and |window_b| should no longer be
  // occluded.
  auto shape = std::make_unique<ui::Layer::ShapeRects>();
  shape->emplace_back(0, 0, 5, 5);
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->layer()->SetAlphaShape(std::move(shape));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Clear the shape for |window_b|. |window_a| and |window_b| should be
  // occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  window_b->layer()->SetAlphaShape(nullptr);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

namespace {

class WindowDelegateHidingWindow : public MockWindowDelegate {
 public:
  WindowDelegateHidingWindow() = default;

  WindowDelegateHidingWindow(const WindowDelegateHidingWindow&) = delete;
  WindowDelegateHidingWindow& operator=(const WindowDelegateHidingWindow&) =
      delete;

  void set_window_to_update(Window* window) { window_to_update_ = window; }

  // MockWindowDelegate:
  void OnWindowOcclusionChanged(
      Window::OcclusionState old_occlusion_state,
      Window::OcclusionState new_occlusion_state) override {
    MockWindowDelegate::OnWindowOcclusionChanged(old_occlusion_state,
                                                 new_occlusion_state);
    if (!window_to_update_)
      return;

    window_to_update_->Hide();
  }

 private:
  raw_ptr<Window, DanglingUntriaged> window_to_update_ = nullptr;
};

class WindowDelegateAddingAndHidingChild : public MockWindowDelegate {
 public:
  explicit WindowDelegateAddingAndHidingChild(WindowOcclusionTrackerTest* test)
      : test_(test) {}

  WindowDelegateAddingAndHidingChild(
      const WindowDelegateAddingAndHidingChild&) = delete;
  WindowDelegateAddingAndHidingChild& operator=(
      const WindowDelegateAddingAndHidingChild&) = delete;

  void set_queued_expectation(Window::OcclusionState occlusion_state,
                              const SkRegion& occluded_region) {
    queued_expected_occlusion_state_ = occlusion_state;
    queued_expected_occluded_region_ = occluded_region;
  }

  void set_window_to_update(Window* window) { window_to_update_ = window; }

  // MockWindowDelegate:
  void OnWindowOcclusionChanged(
      Window::OcclusionState old_occlusion_state,
      Window::OcclusionState new_occlusion_state) override {
    MockWindowDelegate::OnWindowOcclusionChanged(old_occlusion_state,
                                                 new_occlusion_state);
    if (queued_expected_occlusion_state_ != Window::OcclusionState::UNKNOWN) {
      set_expectation(queued_expected_occlusion_state_,
                      queued_expected_occluded_region_);
      queued_expected_occlusion_state_ = Window::OcclusionState::UNKNOWN;
      queued_expected_occluded_region_ = SkRegion();
    }

    if (!window_to_update_)
      return;

    // Create a child window and hide it. Since this code runs when occlusion
    // has already been recomputed twice, if one of the operations below causes
    // occlusion to be recomputed, the test will fail with a DCHECK.
    Window* window =
        test_->CreateUntrackedWindow(gfx::Rect(0, 0, 5, 5), window_to_update_);
    window->Hide();
  }

 private:
  raw_ptr<WindowOcclusionTrackerTest> test_;
  raw_ptr<Window> window_to_update_ = nullptr;
  Window::OcclusionState queued_expected_occlusion_state_ =
      Window::OcclusionState::UNKNOWN;
  SkRegion queued_expected_occluded_region_ = SkRegion();
};

}  // namespace

// Verify that hiding a window that has a hidden parent doesn't cause occlusion
// to be recomputed.
TEST_F(WindowOcclusionTrackerTest,
       HideWindowWithHiddenParentOnOcclusionChange) {
  test::WindowOcclusionTrackerTestApi test_api(
      Env::GetInstance()->GetWindowOcclusionTracker());

  auto* delegate_a = new WindowDelegateAddingAndHidingChild(this);
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  auto* delegate_b = new WindowDelegateHidingWindow();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 10, 10, 10)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(0, 10, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // When |window_b| is hidden, it will hide |window_a|. |window_a| will in turn
  // add a child to itself and hide it.
  delegate_a->set_window_to_update(window_a);
  delegate_b->set_window_to_update(window_a);
  // Initially A is marked as visible with no potential occlusion.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delegate_a->set_queued_expectation(Window::OcclusionState::HIDDEN,
                                     SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
  // Hiding a child to |window_a| and hiding it shouldn't cause occlusion to be
  // recomputed too many times (i.e. the call below shouldn't DCHECK).
  window_b->Hide();
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that hiding a window changes the occlusion region to show that the
// window is fully occluded.
TEST_F(WindowOcclusionTrackerTest,
       HideWindowChangesOcclusionRegionToBeFullyOccluded) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
  window_a->Hide();
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Test partial occlusion, test partial occlusion changing hidden, alpha shape
// occlusion from multiple windows

// Verify that a window can occlude another one partially.
TEST_F(WindowOcclusionTrackerTest, WindowOccludesWindowPartially) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 20, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b, occluding window a partially.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(6, 7, 8, 9)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(6, 7, 8, 9));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Hiding window b should stop occluding window a partially.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delegate_b->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
  window_b->Hide();
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());
}

// Verify that windows with alpha shape do not affect occlusion regions.
TEST_F(WindowOcclusionTrackerTest,
       WindowWithAlphaShapeDoesNotPartiallyOccludeOtherWindows) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 20, 20));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b, occluding window a partially.
  MockWindowDelegate* delegate_b = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(6, 7, 8, 9)));
  delegate_b->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_b = CreateTrackedWindow(delegate_b, gfx::Rect(6, 7, 8, 9));
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_b->is_expecting_call());

  // Set a shape for window b. The window underneath should no longer be
  // partially occluded.
  auto shape = std::make_unique<ui::Layer::ShapeRects>();
  shape->emplace_back(0, 0, 5, 5);
  // Shaped windows are not considered opaque, so the occluded region is empty.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->layer()->SetAlphaShape(std::move(shape));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Clear the shape for the top window. The window underneath should be
  // occluded.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(6, 7, 8, 9)));
  window_b->layer()->SetAlphaShape(nullptr);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that a window can be occluded by multiple other windows.
TEST_F(WindowOcclusionTrackerTest, WindowCanBeOccludedByMultipleWindows) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(5, 5, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  SkRegion window_a_occlusion = SkRegion(SkIRect::MakeXYWH(14, 14, 5, 5));
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              window_a_occlusion);
  CreateUntrackedWindow(gfx::Rect(14, 14, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_a_occlusion.op(SkIRect::MakeXYWH(1, 1, 5, 5), SkRegion::Op::kUnion_Op);
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              window_a_occlusion);
  CreateUntrackedWindow(gfx::Rect(1, 1, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_a_occlusion.op(SkIRect::MakeXYWH(14, 1, 5, 5),
                        SkRegion::Op::kUnion_Op);
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              window_a_occlusion);
  CreateUntrackedWindow(gfx::Rect(14, 1, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_a_occlusion.op(SkIRect::MakeXYWH(10, 10, 2, 3),
                        SkRegion::Op::kUnion_Op);
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              window_a_occlusion);
  CreateUntrackedWindow(gfx::Rect(10, 10, 2, 3));
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Verify that the excluded window is indeed ignored by occlusion tracking.
TEST_P(WindowOcclusionTrackerOpacityTest, ExcludeWindow) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->SetName("WindowA");

  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 0, 100, 100), nullptr);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  MockWindowDelegate* delegate_bb = new MockWindowDelegate();
  delegate_bb->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_bb, gfx::Rect(0, 0, 10, 10), window_b);
  EXPECT_FALSE(delegate_bb->is_expecting_call());

  delegate_bb->SetName("WindowBB");

  delegate_bb->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_c = CreateUntrackedWindow(gfx::Rect(0, 0, 100, 100), nullptr);
  EXPECT_FALSE(delegate_bb->is_expecting_call());

  {
    // |window_b| is excluded, so its child's occlusion state becomes VISIBlE.
    delegate_bb->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
    EXPECT_TRUE(delegate_bb->is_expecting_call());
    WindowOcclusionTracker::ScopedExclude scoped(window_b);
    EXPECT_FALSE(delegate_bb->is_expecting_call());

    // Moving |window_c| out from |window_a| will make |window_a| visible
    // because |window_b| is ignored.
    SkRegion window_a_occlusion(SkIRect::MakeXYWH(100, 100, 100, 100));
    delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                                window_a_occlusion);
    SkRegion window_bb_occlusion;
    window_bb_occlusion.op(SkIRect::MakeXYWH(100, 100, 100, 100),
                           SkRegion::kUnion_Op);
    delegate_bb->set_expectation(Window::OcclusionState::VISIBLE,
                                 window_bb_occlusion);
    window_c->SetBounds(gfx::Rect(100, 100, 100, 100));

    // Un-excluding wil make |window_bb| OCCLUDED.
    delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
    delegate_bb->set_expectation(Window::OcclusionState::VISIBLE,
                                 window_bb_occlusion);
  }
  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_bb->is_expecting_call());

  {
    delegate_bb->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
    SkRegion window_a_occlusion(SkIRect::MakeXYWH(100, 100, 100, 100));
    delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                                window_a_occlusion);
    EXPECT_TRUE(delegate_bb->is_expecting_call());
    WindowOcclusionTracker::ScopedExclude scoped(window_b);
    EXPECT_FALSE(delegate_bb->is_expecting_call());
    EXPECT_FALSE(delegate_a->is_expecting_call());

    // Moving |window_b| will not affect the occlusion status.
    window_b->SetBounds(gfx::Rect(5, 5, 100, 100));

    // Un-excluding will update the occlustion status.
    // A's occlustion status includes all windows above a.
    window_a_occlusion.setEmpty();
    window_a_occlusion.op(SkIRect::MakeXYWH(5, 5, 100, 100),
                          SkRegion::kUnion_Op);
    window_a_occlusion.op(SkIRect::MakeXYWH(100, 100, 100, 100),
                          SkRegion::kUnion_Op);
    delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                                window_a_occlusion);

    SkRegion window_bb_occlusion(SkIRect::MakeXYWH(100, 100, 100, 100));
    delegate_bb->set_expectation(Window::OcclusionState::VISIBLE,
                                 window_bb_occlusion);
  }

  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_bb->is_expecting_call());

  {
    delegate_bb->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
    SkRegion window_a_occlusion(SkIRect::MakeXYWH(100, 100, 100, 100));
    delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                                window_a_occlusion);
    EXPECT_TRUE(delegate_bb->is_expecting_call());
    WindowOcclusionTracker::ScopedExclude scoped(window_b);
    EXPECT_FALSE(delegate_bb->is_expecting_call());
    EXPECT_FALSE(delegate_a->is_expecting_call());

    // Deleting the excluded window will un-exclude itself and recomputes the
    // occlustion state, but should not affect the state on existing windows
    // because it's already excluded.
    delete window_b;
    EXPECT_FALSE(scoped.window());
  }

  MockWindowDelegate* delegate_d = new MockWindowDelegate();
  delegate_d->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());

  auto* window_d = CreateTrackedWindow(delegate_d, gfx::Rect(0, 0, 10, 10),
                                       nullptr, false, layer_type());
  window_d->SetName("WindowD");

  EXPECT_FALSE(delegate_a->is_expecting_call());
  EXPECT_FALSE(delegate_d->is_expecting_call());

  {
    // Make sure excluding the tracked window also works.
    SkRegion window_a_occlusion(SkIRect::MakeXYWH(100, 100, 100, 100));
    delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                                window_a_occlusion);
    WindowOcclusionTracker::ScopedExclude scoped(window_d);
    EXPECT_FALSE(delegate_a->is_expecting_call());

    // Changing opacity/bounds shouldn't change the occlusion state.
    SetOpacity(window_d, 0.5f);
    window_d->SetBounds(gfx::Rect(0, 0, 20, 20));

    // A is now visible even if |window_d| is un-excluded because
    // window_d is not fully opaque.
  }

  EXPECT_FALSE(delegate_a->is_expecting_call());
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  SetOpacity(window_d, 1.f);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Test that calling OnOcclusionStateChanged on a root window causes children
// of the root window to have their delegate notified that it is occluded or
// visible, depending on whether the root window is occluded or not.
TEST_F(WindowOcclusionTrackerTest, NativeWindowOcclusion) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  // Make the host call OnOcclusionStateChanged on the root window.
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::OCCLUDED, {});
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  host()->SetNativeWindowOcclusionState(Window::OcclusionState::VISIBLE, {});
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_F(WindowOcclusionTrackerTest, ScopedForceVisible) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
  window->Hide();
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Using ScopedForceVisible when the window is hidden should force it visible.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  {
    WindowOcclusionTracker::ScopedForceVisible force_visible(window);
    EXPECT_FALSE(delegate_a->is_expecting_call());

    // Destroying the ScopedForceVisible should return the window to hidden.
    delegate_a->set_expectation(Window::OcclusionState::HIDDEN, SkRegion());
  }
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_F(WindowOcclusionTrackerTest, ScopedForceVisibleSiblingsIgnored) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  CreateUntrackedWindow(gfx::Rect(0, 0, 100, 100), nullptr);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Using ScopedForceVisible when the window is occluded should force it
  // visible.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  {
    WindowOcclusionTracker::ScopedForceVisible force_visible(window);
    EXPECT_FALSE(delegate_a->is_expecting_call());

    // Destroying the ScopedForceVisible should return the window to hidden.
    delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  }
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_F(WindowOcclusionTrackerTest, ScopedForceVisibleWithOccludedSibling) {
  // Creates three windows, a parent with two children. Both children have
  // the same bounds.
  std::unique_ptr<WindowOcclusionTracker::ScopedPause>
      pause_occlusion_tracking =
          std::make_unique<WindowOcclusionTracker::ScopedPause>();
  MockWindowDelegate* parent_delegate = new MockWindowDelegate();
  Window* parent_window =
      CreateTrackedWindow(parent_delegate, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(parent_delegate->is_expecting_call());
  MockWindowDelegate* occluded_child_delegate = new MockWindowDelegate();
  CreateTrackedWindow(occluded_child_delegate, gfx::Rect(0, 0, 10, 10),
                      parent_window);
  EXPECT_FALSE(occluded_child_delegate->is_expecting_call());
  MockWindowDelegate* visible_child_delegate = new MockWindowDelegate();
  CreateTrackedWindow(visible_child_delegate, gfx::Rect(0, 0, 10, 10),
                      parent_window);
  EXPECT_FALSE(visible_child_delegate->is_expecting_call());

  // Initial state after creation.
  parent_delegate->set_expectation(Window::OcclusionState::VISIBLE);
  occluded_child_delegate->set_expectation(Window::OcclusionState::OCCLUDED);
  visible_child_delegate->set_expectation(Window::OcclusionState::VISIBLE);
  pause_occlusion_tracking.reset();
  EXPECT_FALSE(parent_delegate->is_expecting_call());
  EXPECT_FALSE(occluded_child_delegate->is_expecting_call());
  EXPECT_FALSE(visible_child_delegate->is_expecting_call());

  // Hiding the parent should result in all windows being hidden.
  parent_delegate->set_expectation(Window::OcclusionState::HIDDEN);
  occluded_child_delegate->set_expectation(Window::OcclusionState::HIDDEN);
  visible_child_delegate->set_expectation(Window::OcclusionState::HIDDEN);
  parent_window->Hide();
  EXPECT_FALSE(parent_delegate->is_expecting_call());
  EXPECT_FALSE(occluded_child_delegate->is_expecting_call());
  EXPECT_FALSE(visible_child_delegate->is_expecting_call());

  // Creating a ScopedForceVisible for the parent should return to the initial
  // state.
  parent_delegate->set_expectation(Window::OcclusionState::VISIBLE);
  occluded_child_delegate->set_expectation(Window::OcclusionState::OCCLUDED);
  visible_child_delegate->set_expectation(Window::OcclusionState::VISIBLE);
  WindowOcclusionTracker::ScopedForceVisible force_visible(parent_window);
  EXPECT_FALSE(parent_delegate->is_expecting_call());
  EXPECT_FALSE(occluded_child_delegate->is_expecting_call());
  EXPECT_FALSE(visible_child_delegate->is_expecting_call());

  // Do another show, so that once the |force_visible| is destroyed the
  // assertions in MockWindowDelegate aren't tripped.
  parent_window->Show();
}

// Simulates a scenario in which a browser window is forced visible (e.g. while
// projecting) and its parent container (e.g. a virtual desks container) was
// hidden. Verifies that the browser window and its descendants remain visible
// from an occlusion stand point.
TEST_F(WindowOcclusionTrackerTest, ScopedForceVisibleHiddenContainer) {
  std::unique_ptr<WindowOcclusionTracker::ScopedPause>
      pause_occlusion_tracking =
          std::make_unique<WindowOcclusionTracker::ScopedPause>();
  MockWindowDelegate* root_delegate = new MockWindowDelegate();
  Window* root = CreateTrackedWindow(root_delegate, gfx::Rect(0, 0, 100, 100));
  MockWindowDelegate* container_delegate = new MockWindowDelegate();
  Window* container =
      CreateTrackedWindow(container_delegate, gfx::Rect(0, 0, 100, 100), root);
  MockWindowDelegate* browser_delegate = new MockWindowDelegate();
  Window* browser =
      CreateTrackedWindow(browser_delegate, gfx::Rect(0, 0, 10, 10), container);
  MockWindowDelegate* webcontents_delegate = new MockWindowDelegate();
  Window* webcontents = CreateTrackedWindow(webcontents_delegate,
                                            gfx::Rect(0, 0, 10, 10), browser);
  root_delegate->set_expectation(Window::OcclusionState::VISIBLE);
  container_delegate->set_expectation(Window::OcclusionState::VISIBLE);
  browser_delegate->set_expectation(Window::OcclusionState::VISIBLE);
  webcontents_delegate->set_expectation(Window::OcclusionState::VISIBLE);
  pause_occlusion_tracking.reset();
  EXPECT_FALSE(root_delegate->is_expecting_call());
  EXPECT_FALSE(container_delegate->is_expecting_call());
  EXPECT_FALSE(browser_delegate->is_expecting_call());
  EXPECT_FALSE(webcontents_delegate->is_expecting_call());

  WindowOcclusionTracker::ScopedForceVisible force_visible(browser);
  container_delegate->set_expectation(Window::OcclusionState::HIDDEN);
  container->Hide();
  EXPECT_FALSE(root_delegate->is_expecting_call());
  EXPECT_FALSE(container_delegate->is_expecting_call());
  EXPECT_FALSE(browser_delegate->is_expecting_call());
  EXPECT_FALSE(webcontents_delegate->is_expecting_call());

  EXPECT_EQ(Window::OcclusionState::VISIBLE, webcontents->GetOcclusionState());
  EXPECT_TRUE(webcontents->TargetVisibility());

  container_delegate->set_expectation(Window::OcclusionState::VISIBLE);
  container->Show();
}

TEST_F(WindowOcclusionTrackerTest, ComputeTargetOcclusionForWindow) {
  auto* window_a = CreateUntrackedWindow(gfx::Rect(5, 5, 10, 10));
  CreateUntrackedWindow(gfx::Rect(14, 14, 5, 5));
  CreateUntrackedWindow(gfx::Rect(1, 1, 5, 5));
  CreateUntrackedWindow(gfx::Rect(14, 1, 5, 5));
  CreateUntrackedWindow(gfx::Rect(10, 10, 2, 3));

  SkRegion window_a_occlusion = SkRegionFromSkIRects(
      {SkIRect::MakeXYWH(14, 14, 5, 5), SkIRect::MakeXYWH(1, 1, 5, 5),
       SkIRect::MakeXYWH(14, 1, 5, 5), SkIRect::MakeXYWH(10, 10, 2, 3)});

  auto& occlusion_tracker = GetOcclusionTracker();
  window_a->TrackOcclusionState();
  auto occlusion_data =
      occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::VISIBLE, occlusion_data.occlusion_state);
  EXPECT_EQ(window_a_occlusion, occlusion_data.occluded_region);
}

TEST_F(WindowOcclusionTrackerTest,
       ComputeTargetOcclusionForWindowUsesTargetBounds) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(10, 10, 10, 10)));
  Window* window_b = CreateUntrackedWindow(gfx::Rect(10, 10, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Should be visible for target occlusion.
  auto& occlusion_tracker = GetOcclusionTracker();
  auto occlusion_data =
      occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::VISIBLE, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(SkIRect::MakeXYWH(10, 10, 10, 10)),
            occlusion_data.occluded_region);

  // Start animating |window_b| to fully occlude |window_a|.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  window_b->layer()->SetAnimator(test_controller.animator());
  window_b->SetBounds(gfx::Rect(0, 0, 10, 10));

  // Animated windows are ignored by the occlusion tracker.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Target occlusion should include them, however:
  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::OCCLUDED, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  // Don't expect the target occlusion to be affected by progress through
  // the animation.
  test_controller.Step(kTransitionDuration / 3);
  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::OCCLUDED, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  // Finish the animation, and expect the occlusion state to update.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  test_controller.Step(kTransitionDuration / 3);
  window_b->layer()->SetAnimator(nullptr);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_P(WindowOcclusionTrackerOpacityTest,
       ComputeTargetOcclusionForWindowUsesTargetOpacity) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_b =
      CreateUntrackedWindow(gfx::Rect(0, 0, 10, 10), nullptr, layer_type());
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  SetOpacity(window_b, 0.0f);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Should be visible for target occlusion.
  auto& occlusion_tracker = GetOcclusionTracker();
  auto occlusion_data =
      occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::VISIBLE, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  // Start animating |window_b| to fully occlude |window_a|.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  window_b->layer()->SetAnimator(test_controller.animator());
  SetOpacity(window_b, 1.0f);

  // Opacity animation uses threaded animation.
  test_controller.StartThreadedAnimationsIfNeeded();

  // Animated windows are ignored by the occlusion tracker.
  test_controller.Step(kTransitionDuration / 3);

  // Target occlusion should include them, however:
  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::OCCLUDED, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  // Don't expect the target occlusion to be affected by progress through
  // the animation.
  test_controller.Step(kTransitionDuration / 3);
  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::OCCLUDED, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  // Finish the animation, and expect the occlusion state to update.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  test_controller.Step(kTransitionDuration / 3);
  // Explicitly stop animation because threaded animation may have started
  // a bit later. |kTransitionDuration| may not be quite enough to reach the
  // end.
  window_b->layer()->GetAnimator()->StopAnimating();
  window_b->layer()->SetAnimator(nullptr);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_F(WindowOcclusionTrackerTest,
       ComputeTargetOcclusionForWindowUsesTargetVisibility) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Hide the window.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->Hide();
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Should be visible for target occlusion.
  auto& occlusion_tracker = GetOcclusionTracker();
  auto occlusion_data =
      occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::VISIBLE, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  // Start animating |window_b| to fully occlude |window_a|.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  window_b->layer()->SetAnimator(test_controller.animator());
  window_b->Show();

  // Animated windows are ignored by the occlusion tracker.
  test_controller.Step(kTransitionDuration / 3);

  // Target occlusion should include them, however:
  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::OCCLUDED, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  // Don't expect the target occlusion to be affected by progress through
  // the animation.
  test_controller.Step(kTransitionDuration / 3);
  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::OCCLUDED, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  // Finish the animation, and expect the occlusion state to update.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  test_controller.Step(kTransitionDuration / 3);
  window_b->layer()->SetAnimator(nullptr);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_F(WindowOcclusionTrackerTest,
       ComputeTargetOcclusionForWindowTransformHierarchy) {
  gfx::Transform scale_2x_transform;
  scale_2x_transform.Scale(2.0f, 2.0f);

  // Effective bounds: x = 2, y = 2, height = 10, width = 10
  Window* window_a = CreateUntrackedWindow(gfx::Rect(2, 2, 5, 5));
  window_a->SetTransform(scale_2x_transform);
  // Effective bounds: x = 4, y = 4, height = 4, width = 4
  Window* window_b = CreateUntrackedWindow(gfx::Rect(1, 1, 2, 2), window_a);
  // Effective bounds: x = 34, y = 36, height = 8, width = 10
  CreateUntrackedWindow(gfx::Rect(15, 16, 4, 5), window_b);

  Window* window_d = CreateUntrackedWindow(gfx::Rect(34, 36, 8, 10));

  auto& occlusion_tracker = GetOcclusionTracker();
  window_d->TrackOcclusionState();
  auto occlusion_data =
      occlusion_tracker.ComputeTargetOcclusionForWindow(window_d);
  EXPECT_EQ(Window::OcclusionState::VISIBLE, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  root_window()->StackChildAtBottom(window_d);

  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_d);
  EXPECT_EQ(Window::OcclusionState::OCCLUDED, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  window_d->SetBounds(gfx::Rect(63, 72, 8, 10));

  SkRegion occluded_area = SkRegionFromSkIRects(
      {SkIRect::MakeXYWH(2, 2, 10, 10), SkIRect::MakeXYWH(4, 4, 4, 4),
       SkIRect::MakeXYWH(34, 36, 8, 10)});
  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_d);
  EXPECT_EQ(Window::OcclusionState::VISIBLE, occlusion_data.occlusion_state);
  EXPECT_EQ(occluded_area, occlusion_data.occluded_region);

  // Set a target transform on |window_b| which should increase the size of
  // its child window, occluding |window_d|.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  // Scale |window_b|.
  // |window_b| effective bounds: 4,4 8x8
  // |window_b|'s child's effective bounds: 64,68 16x20
  window_b->layer()->SetAnimator(test_controller.animator());
  window_b->layer()->SetTransform(scale_2x_transform);

  // Transform animation uses threaded animation.
  test_controller.StartThreadedAnimationsIfNeeded();

  // Animated windows are ignored by the occlusion tracker.
  test_controller.Step(kTransitionDuration / 3);

  // Target occlusion should include them, however:
  SkRegion occluded_area_transformed = SkRegionFromSkIRects(
      {SkIRect::MakeXYWH(2, 2, 10, 10), SkIRect::MakeXYWH(4, 4, 8, 8),
       SkIRect::MakeXYWH(64, 68, 16, 20)});
  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_d);
  EXPECT_EQ(Window::OcclusionState::VISIBLE, occlusion_data.occlusion_state);
  EXPECT_EQ(occluded_area_transformed, occlusion_data.occluded_region);

  // Don't expect the target occlusion to be affected by progress through
  // the animation.
  test_controller.Step(kTransitionDuration / 3);
  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_d);
  EXPECT_EQ(Window::OcclusionState::VISIBLE, occlusion_data.occlusion_state);
  EXPECT_EQ(occluded_area_transformed, occlusion_data.occluded_region);
  window_b->layer()->GetAnimator()->StopAnimating();
  window_b->layer()->SetAnimator(nullptr);
}

TEST_F(WindowOcclusionTrackerTest, ComputeTargetOcclusionForAnimatedWindow) {
  // Computing the target occlusion for an animated window should use the
  // target values, and therefore compute the target occlusion for the animated
  // window at its final location in this test.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(10, 10, 10, 10)));
  Window* window_b = CreateUntrackedWindow(gfx::Rect(10, 10, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Start animating |window_a| to be fully occluded by |window_b|.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::LayerAnimatorTestController test_controller(
      ui::LayerAnimator::CreateImplicitAnimator());
  ui::ScopedLayerAnimationSettings layer_animation_settings(
      test_controller.animator());
  layer_animation_settings.SetTransitionDuration(kTransitionDuration);

  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_a->layer()->SetAnimator(test_controller.animator());
  window_a->SetBounds(gfx::Rect(10, 10, 10, 10));
  test_controller.Step(kTransitionDuration / 3);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Target occlusion should take into account the final bounds.
  auto& occlusion_tracker = GetOcclusionTracker();
  auto occlusion_data =
      occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::OCCLUDED, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  // Don't expect the target occlusion to be affected by progress through
  // the animation.
  test_controller.Step(kTransitionDuration / 3);
  occlusion_data = occlusion_tracker.ComputeTargetOcclusionForWindow(window_a);
  EXPECT_EQ(Window::OcclusionState::OCCLUDED, occlusion_data.occlusion_state);
  EXPECT_EQ(SkRegion(), occlusion_data.occluded_region);

  // Finish the animation, and expect the occlusion state to update.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  test_controller.Step(kTransitionDuration / 3);
  window_b->layer()->SetAnimator(nullptr);
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_F(WindowOcclusionTrackerTest,
       SetOpaqueRegionsForOcclusionAffectsOcclusionOfOtherWindows) {
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(10, 10, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_b = CreateUntrackedWindow(gfx::Rect(10, 10, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Make |window_b| transparent, which should make it no longer affect
  // |window_a|'s occlusion.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->SetTransparent(true);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Set the opaque regions for occlusion to fully cover |window_a|. Opaque
  // regions for occlusion are relative to the window.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  window_b->SetOpaqueRegionsForOcclusion({gfx::Rect(0, 0, 10, 10)});
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Setting the opaque regions for occlusion to an empty list should restore to
  // normal behavior:
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->SetOpaqueRegionsForOcclusion({});
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_F(
    WindowOcclusionTrackerTest,
    SetOpaqueRegionsForOcclusionOfAWindowDoesNotAffectOcclusionOfThatWindowItself) {
  // The opaque regions for occlusion of a window affect how that window
  // occludes other windows, but should not affect occlusion for that window
  // itself. This is because occluding only the opaque regions of occlusion for
  // a window may still leave translucent parts of that window visible.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(5, 5, 5, 5)));
  CreateUntrackedWindow(gfx::Rect(5, 5, 5, 5));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  window_a->SetTransparent(true);
  // Changing the opaque regions for occlusion should not affect how much
  // |window_a| is occluded by |window_b|.
  window_a->SetOpaqueRegionsForOcclusion({gfx::Rect(0, 0, 0, 0)});
  window_a->SetOpaqueRegionsForOcclusion({gfx::Rect(0, 0, 1, 1)});
  window_a->SetOpaqueRegionsForOcclusion({gfx::Rect(0, 0, 5, 5)});
  window_a->SetOpaqueRegionsForOcclusion({});
}

TEST_F(WindowOcclusionTrackerTest,
       SemiOpaqueSolidColorLayerDoesNotAffectChildOpacity) {
  // Create window a. Expect it to be non-occluded.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 10, 10));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Create window b. Expect it to be non-occluded and expect window a to be
  // occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 0, 15, 15), nullptr,
                                           ui::LAYER_SOLID_COLOR);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Semi-opaque color on the window_b should make window a visible.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->layer()->SetColor(SkColorSetARGB(127, 255, 255, 255));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // Creating opaque layer on top of a half-opaque solid_color layer
  // can occlude the window.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_c = CreateUntrackedWindow(gfx::Rect(0, 0, 15, 15), window_b);
  EXPECT_FALSE(delegate_a->is_expecting_call());
  DCHECK(window_c);

  // Removing the opaque layer should make the window_a visible.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  delete window_c;
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_F(WindowOcclusionTrackerTest, OccludedFractionalWindow) {
  // Test that a window which gets a fractional scale after a transform is
  // treated as its floored size when being occluded i.e. a 6.875x6.875 window
  // gets occluded by a 6x6 window. Read comment on
  // |WindowOcclusionTracker::RecomputeOcclusionImpl()| to understand why we do
  // this.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 11, 11));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // 11 * 0.625 = 0.6875
  window_a->SetTransform(gfx::Transform::MakeScale(0.625f));

  // Since `window_a` is treated as a 6x6 window, it gets marked as occluded.
  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  CreateUntrackedWindow(gfx::Rect(0, 0, 6, 6));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  // 12 * 0.625 = 7.5
  // Now `window_a` is treated as 7x7 window and thus cannot be occluded by 6x6
  // window.
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 0, 6, 6)));
  window_a->SetBounds(gfx::Rect(0, 0, 12, 12));
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_F(WindowOcclusionTrackerTest, OccludingFractionalWindow) {
  // Test that a window which gets a fractional scale after a transform is
  // treated as its ceiled value when occluding other windows i.e.
  // a 10.625x10.625 window occludes an 11x11 window. Read comment on
  // |WindowOcclusionTracker::RecomputeOcclusionImpl()| to understand why we do
  // this.
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  Window* window_a = CreateTrackedWindow(delegate_a, gfx::Rect(0, 0, 11, 11));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  Window* window_b = CreateUntrackedWindow(gfx::Rect(0, 0, 17, 17));
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  window_b->SetTransparent(true);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  // 17 * 0.625 = 10.625
  // `window_b` occludes `window_a` of size 11x11.
  window_b->SetTransform(gfx::Transform::MakeScale(0.625f));
  window_b->SetOpaqueRegionsForOcclusion({gfx::Rect(0, 0, 17, 17)});
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::VISIBLE,
                              SkRegion(SkIRect::MakeXYWH(0, 0, 11, 11)));
  window_a->SetBounds(gfx::Rect(0, 0, 12, 12));
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

TEST_F(WindowOcclusionTrackerTest, ClipToRootWindow) {
  // Test that a window larger than the root window is occluded by a window the
  // same size as the root window.
  auto outside_root_window_bounds = root_window()->bounds();
  outside_root_window_bounds.Outset(100);
  MockWindowDelegate* delegate_a = new MockWindowDelegate();
  delegate_a->set_expectation(Window::OcclusionState::VISIBLE, SkRegion());
  CreateTrackedWindow(delegate_a, outside_root_window_bounds);
  EXPECT_FALSE(delegate_a->is_expecting_call());

  delegate_a->set_expectation(Window::OcclusionState::OCCLUDED, SkRegion());
  CreateUntrackedWindow(root_window()->bounds());
  EXPECT_FALSE(delegate_a->is_expecting_call());
}

// Run tests with LAYER_TEXTURE_LAYER type or LAYER_SOLID_COLOR type.
INSTANTIATE_TEST_SUITE_P(All,
                         WindowOcclusionTrackerOpacityTest,
                         testing::Bool());

}  // namespace aura

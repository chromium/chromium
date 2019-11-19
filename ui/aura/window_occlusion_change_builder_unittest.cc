// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_occlusion_change_builder.h"

#include <memory>

#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"

namespace aura {

namespace {

// A delegate that remembers the occlusion info of its window.
class OcclusionTrackWindowDelegate : public test::TestWindowDelegate {
 public:
  OcclusionTrackWindowDelegate() = default;
  ~OcclusionTrackWindowDelegate() override = default;

  bool occlusion_change_count() const { return occlusion_change_count_; }
  Window::OcclusionState last_occlusion_state() const {
    return last_occlusion_state_;
  }
  const SkRegion& last_occluded_region() const { return last_occluded_region_; }

 private:
  // test::TestWindowDelegate:
  void OnWindowOcclusionChanged(Window::OcclusionState occlusion_state,
                                const SkRegion& occluded_region) override {
    ++occlusion_change_count_;
    last_occlusion_state_ = occlusion_state;
    last_occluded_region_ = occluded_region;
  }

  int occlusion_change_count_ = 0;
  Window::OcclusionState last_occlusion_state_ =
      Window::OcclusionState::UNKNOWN;
  SkRegion last_occluded_region_;

  DISALLOW_COPY_AND_ASSIGN(OcclusionTrackWindowDelegate);
};

}  // namespace

class WindowOcclusionChangeBuilderTest : public test::AuraTestBase {
 public:
  WindowOcclusionChangeBuilderTest() = default;
  ~WindowOcclusionChangeBuilderTest() override = default;

  std::unique_ptr<Window> CreateTestWindow(
      OcclusionTrackWindowDelegate* delegate) {
    auto window = std::make_unique<Window>(delegate);
    window->set_owned_by_parent(false);
    window->SetType(client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->Show();

    root_window()->AddChild(window.get());
    return window;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowOcclusionChangeBuilderTest);
};

// Test that window occlusion info is updated after commit.
TEST_F(WindowOcclusionChangeBuilderTest, SingleWindow) {
  SkRegion region;
  region.setRect({1, 2, 3, 4});

  for (const auto state :
       {Window::OcclusionState::VISIBLE, Window::OcclusionState::OCCLUDED,
        Window::OcclusionState::HIDDEN}) {
    OcclusionTrackWindowDelegate delegate;
    auto window = CreateTestWindow(&delegate);

    auto builder = WindowOcclusionChangeBuilder::Create();
    builder->Add(window.get(), state, region);

    // Change should not be applied before Commit call.
    EXPECT_EQ(0, delegate.occlusion_change_count());

    // All changes are committed when builder is released.
    builder.reset();

    EXPECT_EQ(1, delegate.occlusion_change_count());
    EXPECT_EQ(state, delegate.last_occlusion_state());
    EXPECT_EQ(region, delegate.last_occluded_region());
  }
}

// Test updating multiple windows.
TEST_F(WindowOcclusionChangeBuilderTest, MultipleWindow) {
  auto builder = WindowOcclusionChangeBuilder::Create();

  OcclusionTrackWindowDelegate delegate1;
  auto window1 = CreateTestWindow(&delegate1);
  const Window::OcclusionState state1 = Window::OcclusionState::VISIBLE;
  SkRegion region1;
  region1.setRect({1, 2, 3, 4});
  builder->Add(window1.get(), state1, region1);

  OcclusionTrackWindowDelegate delegate2;
  auto window2 = CreateTestWindow(&delegate2);
  const Window::OcclusionState state2 = Window::OcclusionState::OCCLUDED;
  SkRegion region2;
  region2.setRect({5, 6, 7, 8});
  builder->Add(window2.get(), state2, region2);

  // Changes should not be applied before Commit call.
  EXPECT_EQ(0, delegate1.occlusion_change_count());
  EXPECT_EQ(0, delegate2.occlusion_change_count());

  // All changes are committed when builder is released.
  builder.reset();

  EXPECT_EQ(1, delegate1.occlusion_change_count());
  EXPECT_EQ(state1, delegate1.last_occlusion_state());
  EXPECT_EQ(region1, delegate1.last_occluded_region());

  EXPECT_EQ(1, delegate2.occlusion_change_count());
  EXPECT_EQ(state2, delegate2.last_occlusion_state());
  EXPECT_EQ(region2, delegate2.last_occluded_region());
}

// Tests that the last change wins when there are multiple changes on the same
// window.
TEST_F(WindowOcclusionChangeBuilderTest, MultipleChanges) {
  OcclusionTrackWindowDelegate delegate;
  auto window = CreateTestWindow(&delegate);

  auto builder = WindowOcclusionChangeBuilder::Create();
  builder->Add(window.get(), Window::OcclusionState::VISIBLE, SkRegion());
  builder->Add(window.get(), Window::OcclusionState::HIDDEN, SkRegion());

  SkRegion region;
  region.setRect({1, 2, 3, 4});
  builder->Add(window.get(), Window::OcclusionState::OCCLUDED, region);

  // All changes are committed when builder is released.
  builder.reset();

  EXPECT_EQ(1, delegate.occlusion_change_count());
  EXPECT_EQ(Window::OcclusionState::OCCLUDED, delegate.last_occlusion_state());
  EXPECT_EQ(region, delegate.last_occluded_region());
}

// Test that occlusion info is not updated if window is destroyed before commit.
TEST_F(WindowOcclusionChangeBuilderTest, DestroyBeforeCommit) {
  OcclusionTrackWindowDelegate delegate;
  auto window = CreateTestWindow(&delegate);

  auto builder = WindowOcclusionChangeBuilder::Create();
  builder->Add(window.get(), Window::OcclusionState::VISIBLE, SkRegion());

  // Destroy window before applying the changes.
  window.reset();

  // All changes are committed when builder is released.
  builder.reset();

  // Occlusion info is not updated.
  EXPECT_EQ(0, delegate.occlusion_change_count());
}

}  // namespace aura

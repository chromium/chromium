// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/animating_layout_manager.h"

#include <utility>
#include <vector>

#include "base/scoped_observer.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Returns a size which is the intersection of |size| and the constraints
// provided by |bounds|, if any.
gfx::Size ConstrainSizeToBounds(const gfx::Size& size,
                                const SizeBounds& bounds) {
  return gfx::Size{
      bounds.width() ? std::min(size.width(), *bounds.width()) : size.width(),
      bounds.height() ? std::min(size.height(), *bounds.height())
                      : size.height()};
}

// View that allows directly setting minimum size.
class TestView : public View {
 public:
  using View::View;
  ~TestView() override = default;

  void SetMinimumSize(gfx::Size minimum_size) { minimum_size_ = minimum_size; }
  gfx::Size GetMinimumSize() const override {
    return minimum_size_ ? *minimum_size_ : View::GetMinimumSize();
  }

 private:
  base::Optional<gfx::Size> minimum_size_;
};

class TestLayoutManager : public LayoutManagerBase {
 public:
  void SetLayout(const ProposedLayout& layout) {
    layout_ = layout;
    InvalidateHost(true);
  }

  const ProposedLayout& layout() const { return layout_; }

 protected:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override {
    ProposedLayout actual;
    actual.host_size = ConstrainSizeToBounds(layout_.host_size, size_bounds);
    actual.child_layouts = layout_.child_layouts;
    return actual;
  }

 private:
  ProposedLayout layout_;
};

// Version of FillLayout that ignores invisible views.
class SmartFillLayout : public FillLayout {
 public:
  gfx::Size GetPreferredSize(const View* host) const override {
    if (host->children().empty())
      return gfx::Size();

    gfx::Size preferred_size;
    for (View* child : host->children()) {
      if (child->GetVisible())
        preferred_size.SetToMax(child->GetPreferredSize());
    }
    gfx::Rect rect(preferred_size);
    rect.Inset(-host->GetInsets());
    return rect.size();
  }
};

constexpr gfx::Size kChildViewSize{10, 10};

}  // anonymous namespace

// Test fixture which creates an AnimatingLayoutManager and instruments it so
// the animations can be directly controlled via gfx::AnimationContainerTestApi.
class AnimatingLayoutManagerSteppingTest : public testing::Test {
 public:
  void SetUp() override {
    // Don't use a unique_ptr because derived classes may want to own this view.
    view_ = new View();
    for (int i = 0; i < 3; ++i) {
      auto child = std::make_unique<TestView>();
      child->SetPreferredSize(kChildViewSize);
      children_.push_back(view_->AddChildView(std::move(child)));
    }

    animating_layout_manager_ =
        view_->SetLayoutManager(std::make_unique<AnimatingLayoutManager>());

    // Use linear transitions to make expected values predictable.
    animating_layout_manager_->SetTweenType(gfx::Tween::Type::LINEAR);
    animating_layout_manager_->SetAnimationDuration(
        base::TimeDelta::FromSeconds(1));

    if (UseContainerTestApi()) {
      container_test_api_ = std::make_unique<gfx::AnimationContainerTestApi>(
          animating_layout_manager_->GetAnimationContainerForTesting());
    }

    // These can't be constructed statically since they depend on the child
    // views.
    layout1_ = {{100, 100},
                {{children_[0], true, {5, 5, 10, 10}},
                 {children_[1], false},
                 {children_[2], true, {20, 20, 20, 20}}}};
    layout2_ = {{200, 200},
                {{children_[0], true, {10, 20, 20, 30}},
                 {children_[1], false},
                 {children_[2], true, {10, 100, 10, 10}}}};
  }

  void TearDown() override { DestroyView(); }

  View* view() { return view_; }
  TestView* child(size_t index) const { return children_[index]; }
  size_t num_children() const { return children_.size(); }
  AnimatingLayoutManager* layout() { return animating_layout_manager_; }
  gfx::AnimationContainerTestApi* animation_api() {
    return container_test_api_.get();
  }
  const ProposedLayout& layout1() const { return layout1_; }
  const ProposedLayout& layout2() const { return layout2_; }

  // Replaces one of the children of |view| with a blank TestView.
  // Because child views have e.g. preferred size set by default, in order to
  // use non-default setup this method should be called.
  void ReplaceChild(int index) {
    View* const old_view = children_[index];
    view_->RemoveChildView(old_view);
    delete old_view;
    children_[index] =
        view_->AddChildViewAt(std::make_unique<TestView>(), index);
  }

  void EnsureLayout(const ProposedLayout& expected) {
    for (size_t i = 0; i < expected.child_layouts.size(); ++i) {
      const auto& expected_child = expected.child_layouts[i];
      const View* const child = expected_child.child_view;
      EXPECT_EQ(view_, child->parent()) << " view " << i << " parent differs.";
      EXPECT_EQ(expected_child.visible, child->GetVisible())
          << " view " << i << " visibility.";
      if (expected_child.visible) {
        EXPECT_EQ(expected_child.bounds, child->bounds())
            << " view " << i << " bounds";
      }
    }
  }

  void DestroyView() {
    if (view_) {
      delete view_;
      view_ = nullptr;
    }
  }

  void SizeAndLayout() {
    // If the layout of |view| is invalid or the size changes, this will
    // automatically call |view->Layout()| as well.
    view_->SizeToPreferredSize();
  }

  virtual bool UseContainerTestApi() const { return true; }

 private:
  ProposedLayout layout1_;
  ProposedLayout layout2_;
  View* view_;
  std::vector<TestView*> children_;
  AnimatingLayoutManager* animating_layout_manager_ = nullptr;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<gfx::AnimationContainerTestApi> container_test_api_;
};

TEST_F(AnimatingLayoutManagerSteppingTest, SetLayoutManager_NoAnimation) {
  auto test_layout = std::make_unique<TestLayoutManager>();
  test_layout->SetLayout(layout1());
  layout()->SetShouldAnimateBounds(true);
  layout()->SetTargetLayoutManager(std::move(test_layout));

  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerSteppingTest, ResetLayout_NoAnimation) {
  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();

  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerSteppingTest, HostInvalidate_TriggersAnimation) {
  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  SizeAndLayout();

  // At this point animation should have started, but not proceeded.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       HostInvalidate_AnimateBounds_AnimationProgresses) {
  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  SizeAndLayout();

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  ProposedLayout expected = ProposedLayoutBetween(0.25, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance again.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  expected = layout2();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       HostInvalidate_NoAnimateBounds_NoAnimation) {
  layout()->SetShouldAnimateBounds(false);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  // First layout. Should not be animating.
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Because the desired layout did not change, there is no animation.
  view()->InvalidateLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       HostResize_NoAnimateBounds_NoAnimation) {
  layout()->SetShouldAnimateBounds(false);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  // First layout. Should not be animating.
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Because the size of the host view changed, there is no animation.
  test_layout->SetLayout(layout2());
  SizeAndLayout();
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout2().host_size, view()->size());
  EnsureLayout(layout2());
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       HostInvalidate_NoAnimateBounds_NewLayoutTriggersAnimation) {
  layout()->SetShouldAnimateBounds(false);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  // First layout. Should not be animating.
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Switching to the new layout without changing size will lead to an
  // animation.
  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       HostInvalidate_NoAnimateBounds_AnimationProgresses) {
  layout()->SetShouldAnimateBounds(false);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  // First layout. Should not be animating.
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Switching to the new layout without changing size will lead to an
  // animation.
  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(layout1());

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->Layout();
  ProposedLayout expected = ProposedLayoutBetween(0.25, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected);

  // Advance again.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->Layout();
  expected = ProposedLayoutBetween(0.5, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  expected = layout2();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_MiddleView_ScaleFromZero) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};
  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  const ProposedLayout final_layout{{35, 20},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), false},
                                     {child(2), true, {20, 5, 10, 10}}}};

  child(1)->SetMinimumSize({5, 5});
  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(initial_layout);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  ProposedLayout expected =
      ProposedLayoutBetween(0.25, initial_layout, final_layout);
  DCHECK_EQ(expected.child_layouts[1].child_view, child(1));
  expected.child_layouts[1].visible = true;
  expected.child_layouts[1].bounds = {
      expected.child_layouts[0].bounds.right() + 5,
      initial_layout.child_layouts[1].bounds.y(),
      expected.child_layouts[2].bounds.x() -
          expected.child_layouts[0].bounds.right() - 10,
      initial_layout.child_layouts[1].bounds.height()};
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, initial_layout, final_layout);
  DCHECK_EQ(expected.child_layouts[1].child_view, child(1));
  expected.child_layouts[1].visible = true;
  expected.child_layouts[1].bounds = {
      expected.child_layouts[0].bounds.right() + 5,
      initial_layout.child_layouts[1].bounds.y(),
      expected.child_layouts[2].bounds.x() -
          expected.child_layouts[0].bounds.right() - 10,
      initial_layout.child_layouts[1].bounds.height()};
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.75, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // zero in size so it will disappear.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(final_layout.host_size, view()->size());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_MiddleView_ScaleFromMinimum) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};
  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromMinimum);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  const ProposedLayout final_layout{{35, 20},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), false},
                                     {child(2), true, {20, 5, 10, 10}}}};

  child(1)->SetMinimumSize({5, 5});
  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(initial_layout);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  ProposedLayout expected =
      ProposedLayoutBetween(0.25, initial_layout, final_layout);
  DCHECK_EQ(expected.child_layouts[1].child_view, child(1));
  expected.child_layouts[1].visible = true;
  expected.child_layouts[1].bounds = {
      expected.child_layouts[0].bounds.right() + 5,
      initial_layout.child_layouts[1].bounds.y(),
      expected.child_layouts[2].bounds.x() -
          expected.child_layouts[0].bounds.right() - 10,
      initial_layout.child_layouts[1].bounds.height()};
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // its minimum size so it will disappear.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(final_layout.host_size, view()->size());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_LeadingView_ScaleFromMinimum) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};
  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromMinimum);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  const ProposedLayout final_layout{{35, 20},
                                    {{child(0), false},
                                     {child(1), true, {5, 5, 10, 10}},
                                     {child(2), true, {20, 5, 10, 10}}}};

  child(0)->SetMinimumSize({5, 5});
  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(initial_layout);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  ProposedLayout expected =
      ProposedLayoutBetween(0.25, initial_layout, final_layout);
  DCHECK_EQ(expected.child_layouts[0].child_view, child(0));
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = {
      5, initial_layout.child_layouts[0].bounds.y(),
      expected.child_layouts[1].bounds.x() - 10,
      initial_layout.child_layouts[0].bounds.height()};
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // its minimum size so it will disappear.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(final_layout.host_size, view()->size());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_TrailingView_ScaleFromMinimum_FadeIn) {
  const ProposedLayout initial_layout{{35, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), false}}};

  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromMinimum);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  const ProposedLayout final_layout{{50, 20},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), true, {20, 5, 10, 10}},
                                     {child(2), true, {35, 5, 10, 10}}}};

  child(2)->SetMinimumSize({5, 5});
  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(initial_layout);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // its minimum size so it will not be visible.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.75, initial_layout, final_layout);
  DCHECK_EQ(expected.child_layouts[2].child_view, child(2));
  expected.child_layouts[2].visible = true;
  expected.child_layouts[2].bounds = {
      expected.child_layouts[1].bounds.right() + 5,
      final_layout.child_layouts[2].bounds.y(),
      expected.host_size.width() - expected.child_layouts[1].bounds.right() -
          10,
      final_layout.child_layouts[2].bounds.height()};
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(final_layout.host_size, view()->size());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_TrailingView_ScaleFromMinimum) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};
  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromMinimum);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  const ProposedLayout final_layout{{35, 20},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), true, {20, 5, 10, 10}},
                                     {child(2), false}}};

  child(2)->SetMinimumSize({5, 5});
  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(initial_layout);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  ProposedLayout expected =
      ProposedLayoutBetween(0.25, initial_layout, final_layout);
  DCHECK_EQ(expected.child_layouts[2].child_view, child(2));
  expected.child_layouts[2].visible = true;
  expected.child_layouts[2].bounds = {
      expected.child_layouts[1].bounds.right() + 5,
      initial_layout.child_layouts[2].bounds.y(),
      expected.host_size.width() - expected.child_layouts[1].bounds.right() -
          10,
      initial_layout.child_layouts[2].bounds.height()};
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // its minimum size so it will disappear.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(final_layout.host_size, view()->size());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_TrailingView_ScaleFromMinimum_Vertical) {
  const ProposedLayout initial_layout{{20, 50},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {5, 20, 10, 10}},
                                       {child(2), true, {5, 35, 10, 10}}}};
  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromMinimum);
  layout()->SetOrientation(LayoutOrientation::kVertical);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  const ProposedLayout final_layout{{20, 35},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), true, {5, 20, 10, 10}},
                                     {child(2), false}}};

  child(2)->SetMinimumSize({5, 5});
  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(initial_layout);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->Layout();
  ProposedLayout expected =
      ProposedLayoutBetween(0.25, initial_layout, final_layout);
  DCHECK_EQ(expected.child_layouts[2].child_view, child(2));
  expected.child_layouts[2].visible = true;
  expected.child_layouts[2].bounds = {
      initial_layout.child_layouts[2].bounds.x(),
      expected.child_layouts[1].bounds.bottom() + 5,
      initial_layout.child_layouts[2].bounds.width(),
      expected.host_size.height() - expected.child_layouts[1].bounds.bottom() -
          10};
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected);

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // its minimum size so it will disappear.
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_SlideFromLeading_LastView) {
  const ProposedLayout initial_layout{{35, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), false}}};
  const ProposedLayout final_layout{{50, 20},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), true, {20, 5, 10, 10}},
                                     {child(2), true, {35, 5, 10, 10}}}};

  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kSlideFromLeadingEdge);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  ProposedLayout expected_layout = initial_layout;
  expected_layout.child_layouts[2] = {child(2), true, {20, 5, 10, 10}};
  EnsureLayout(expected_layout);

  // Advance the animation 20%.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  SizeAndLayout();
  expected_layout.host_size = {38, 20};
  expected_layout.child_layouts[2].bounds = {23, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation 60%.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(600));
  SizeAndLayout();
  expected_layout.host_size = {47, 20};
  expected_layout.child_layouts[2].bounds = {32, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  SizeAndLayout();
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_SlideFromLeading_Vertical) {
  const ProposedLayout initial_layout{{20, 35},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {5, 20, 10, 10}},
                                       {child(2), false}}};
  const ProposedLayout final_layout{{20, 50},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), true, {5, 20, 10, 10}},
                                     {child(2), true, {5, 35, 10, 10}}}};

  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kSlideFromLeadingEdge);
  layout()->SetOrientation(LayoutOrientation::kVertical);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  ProposedLayout expected_layout = initial_layout;
  expected_layout.child_layouts[2] = {child(2), true, {5, 20, 10, 10}};
  EnsureLayout(expected_layout);

  // Advance the animation 20%.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  SizeAndLayout();
  expected_layout.host_size = {20, 38};
  expected_layout.child_layouts[2].bounds = {5, 23, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation 60%.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(600));
  SizeAndLayout();
  expected_layout.host_size = {20, 47};
  expected_layout.child_layouts[2].bounds = {5, 32, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  SizeAndLayout();
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_SlideFromLeading_MiddleView) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 5, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};

  const ProposedLayout final_layout{{35, 20},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), false},
                                     {child(2), true, {20, 5, 10, 10}}}};

  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kSlideFromLeadingEdge);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  ProposedLayout expected_layout = initial_layout;
  EnsureLayout(expected_layout);

  // Advance the animation 20%.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  SizeAndLayout();
  expected_layout.host_size = {47, 20};
  expected_layout.child_layouts[1].bounds = {18, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {32, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation 60%.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(600));
  SizeAndLayout();
  expected_layout.host_size = {38, 20};
  expected_layout.child_layouts[1].bounds = {12, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {23, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  SizeAndLayout();
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_SlideFromLeading_LeadingView_SlidesFromTrailing) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 5, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};

  const ProposedLayout final_layout{{35, 20},
                                    {{child(0), false},
                                     {child(1), true, {5, 5, 5, 10}},
                                     {child(2), true, {20, 5, 10, 10}}}};

  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kSlideFromLeadingEdge);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  ProposedLayout expected_layout = initial_layout;
  EnsureLayout(expected_layout);

  // Advance the animation 20%.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  SizeAndLayout();
  expected_layout.host_size = {38, 20};
  expected_layout.child_layouts[1].bounds = {17, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {32, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation 60%.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(600));
  SizeAndLayout();
  expected_layout.host_size = {47, 20};
  expected_layout.child_layouts[1].bounds = {8, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {23, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  SizeAndLayout();
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       FadeInOutMode_SlideFromTrailing_MiddleView) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 5, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};

  const ProposedLayout final_layout{{35, 20},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), false},
                                     {child(2), true, {20, 5, 10, 10}}}};

  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kSlideFromTrailingEdge);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(final_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  ProposedLayout expected_layout = initial_layout;
  EnsureLayout(expected_layout);

  // Advance the animation 20%.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  SizeAndLayout();
  expected_layout.host_size = {47, 20};
  expected_layout.child_layouts[1].bounds = {20, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {32, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation 60%.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(600));
  SizeAndLayout();
  expected_layout.host_size = {38, 20};
  expected_layout.child_layouts[1].bounds = {20, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {23, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  SizeAndLayout();
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_FadeOutOnVisibilitySet) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_start.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_FadeInOnVisibilitySet) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  child(0)->SetVisible(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  view()->SetSize(expected_end.host_size);
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(true);

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_end.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Regression test for issues: crbug.com/1021332, crbug.com/1003500
TEST_F(AnimatingLayoutManagerSteppingTest,
       FlexLayout_AnimateOutOnDescendentVisbilitySet) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  ReplaceChild(0);
  child(0)->SetLayoutManager(std::make_unique<SmartFillLayout>());
  View* const grandchild = child(0)->AddChildView(std::make_unique<View>());
  grandchild->SetPreferredSize(kChildViewSize);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  grandchild->SetVisible(false);

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_start.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Regression test for issues: crbug.com/1021332, crbug.com/1003500
TEST_F(AnimatingLayoutManagerSteppingTest,
       FlexLayout_AnimateInOnDescendentVisbilitySet) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  ReplaceChild(0);
  child(0)->SetLayoutManager(std::make_unique<SmartFillLayout>());
  View* const grandchild = child(0)->AddChildView(std::make_unique<View>());
  grandchild->SetPreferredSize(kChildViewSize);
  grandchild->SetVisible(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  view()->SetSize(expected_end.host_size);
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  grandchild->SetVisible(true);

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_end.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_FadeInOnAdded) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  view()->RemoveChildView(child(0));
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout after_add{{50, 20},
                                 {{child(0), false},
                                  {child(1), true, {5, 5, 25, 10}},
                                  {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  view()->SetSize(expected_end.host_size);
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  view()->AddChildViewAt(child(0), 0);

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(after_add);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected = ProposedLayoutBetween(0.5, after_add, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_end.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_FadeIn) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  child(0)->SetVisible(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  view()->SetSize(expected_end.host_size);
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  layout()->FadeIn(child(0));

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_end.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_FadeOut) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  layout()->FadeOut(child(0));

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_start.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_FadeOut_NoCrashOnRemove) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout after_remove{
      {50, 20},
      {{child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  View* const removed = child(0);
  view()->RemoveChildView(removed);
  delete removed;

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(after_remove);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, after_remove, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_FadeOut_IgnoreChildView) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  layout()->SetChildViewIgnoredByLayout(child(0), true);

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Test that when one view can flex to fill the space yielded by another view
// which is hidden, and that such a layout change triggers animation.
TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_SlideAfterViewHidden) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.0, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  expected = ProposedLayoutBetween(0.5, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Test that when one view can flex to fill the space yielded by another view
// which is removed, and that such a layout change triggers animation.
TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_SlideAfterViewRemoved) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  view()->RemoveChildView(child(0));
  delete child(0);

  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.0, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  expected = ProposedLayoutBetween(0.5, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Test that when an animation starts and then the target changes mid-stream,
// the animation redirects.
TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_RedirectAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end1{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end2{
      {50, 20},
      {{child(0), false}, {child(1), true, {5, 5, 40, 10}}, {child(2), false}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end1);
  EnsureLayout(expected);

  child(2)->SetVisible(false);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  expected = ProposedLayoutBetween(0.5, expected, expected_end2);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(250));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end2);
}

// Test that when an animation starts and then the target changes near the end
// of the animation, the animation resets.
TEST_F(AnimatingLayoutManagerSteppingTest, FlexLayout_ResetAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end1{
      {50, 20},
      {{child(0), false},
       {child(1), true, {5, 5, 25, 10}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end2{
      {50, 20},
      {{child(0), false}, {child(1), true, {5, 5, 40, 10}}, {child(2), false}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(900));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.9, expected_start, expected_end1);
  EnsureLayout(expected);

  child(2)->SetVisible(false);
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  expected = ProposedLayoutBetween(0.0, expected, expected_end2);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_TRUE(layout()->is_animating());
  expected = ProposedLayoutBetween(0.5, expected, expected_end2);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end2);
}

TEST_F(AnimatingLayoutManagerSteppingTest, TestEvents) {
  class EventWatcher : public AnimatingLayoutManager::Observer {
   public:
    ~EventWatcher() override {}

    explicit EventWatcher(AnimatingLayoutManager* layout) {
      scoped_observer_.Add(layout);
    }

    void OnLayoutIsAnimatingChanged(AnimatingLayoutManager* source,
                                    bool is_animating) override {
      events_.push_back(is_animating);
    }

    const std::vector<bool> events() const { return events_; }

   private:
    std::vector<bool> events_;
    ScopedObserver<AnimatingLayoutManager, Observer> scoped_observer_{this};
  };

  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  EXPECT_FALSE(layout()->is_animating());
  EventWatcher watcher(layout());
  test_layout->SetLayout(layout2());

  // Invalidating the layout forces a recalculation, which starts the animation.
  const std::vector<bool> expected1{true};
  view()->InvalidateLayout();
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected1, watcher.events());

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected1, watcher.events());

  // Final layout clears the |is_animating| state because the views are now in
  // their final configuration.
  const std::vector<bool> expected2{true, false};
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(expected2, watcher.events());
}

TEST_F(AnimatingLayoutManagerSteppingTest, QueueDelayedAction) {
  bool action1_called = false;
  bool action2_called = false;
  auto action1 =
      base::BindOnce([](bool* var) { *var = true; }, &action1_called);
  auto action2 =
      base::BindOnce([](bool* var) { *var = true; }, &action2_called);

  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  EXPECT_FALSE(layout()->is_animating());
  test_layout->SetLayout(layout2());

  // Invalidating the layout forces a recalculation, which starts the animation.
  view()->InvalidateLayout();
  layout()->QueueDelayedAction(std::move(action1));
  layout()->QueueDelayedAction(std::move(action2));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance partially.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Final layout clears the |is_animating| state because the views are now in
  // their final configuration.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(action1_called);
  EXPECT_TRUE(action2_called);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       QueueDelayedAction_ContinueAnimation) {
  bool action1_called = false;
  bool action2_called = false;
  auto action1 =
      base::BindOnce([](bool* var) { *var = true; }, &action1_called);
  auto action2 =
      base::BindOnce([](bool* var) { *var = true; }, &action2_called);

  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  EXPECT_FALSE(layout()->is_animating());
  test_layout->SetLayout(layout2());

  // Invalidating the layout forces a recalculation, which starts the animation.
  view()->InvalidateLayout();
  layout()->QueueDelayedAction(std::move(action1));
  layout()->QueueDelayedAction(std::move(action2));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance partially.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(850));
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Redirect the layout.
  test_layout->SetLayout(layout1());
  view()->InvalidateLayout();

  // Advance partially.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Final layout clears the |is_animating| state because the views are now in
  // their final configuration.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(action1_called);
  EXPECT_TRUE(action2_called);
}

TEST_F(AnimatingLayoutManagerSteppingTest, QueueDelayedAction_NeverFinishes) {
  bool action1_called = false;
  bool action2_called = false;
  auto action1 =
      base::BindOnce([](bool* var) { *var = true; }, &action1_called);
  auto action2 =
      base::BindOnce([](bool* var) { *var = true; }, &action2_called);

  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  EXPECT_FALSE(layout()->is_animating());
  test_layout->SetLayout(layout2());

  // Invalidating the layout forces a recalculation, which starts the animation.
  view()->InvalidateLayout();
  layout()->QueueDelayedAction(std::move(action1));
  layout()->QueueDelayedAction(std::move(action2));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance partially.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Destroy the view and the layout manager. Verify the queued actions are
  // never called (and nothing crashes).
  DestroyView();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);
}

TEST_F(AnimatingLayoutManagerSteppingTest, RunOrQueueAction) {
  bool action1_called = false;
  bool action2_called = false;
  auto action1 =
      base::BindOnce([](bool* var) { *var = true; }, &action1_called);
  auto action2 =
      base::BindOnce([](bool* var) { *var = true; }, &action2_called);

  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  // Since the layout is not animating yet, this action runs immediately.
  EXPECT_FALSE(layout()->is_animating());
  layout()->RunOrQueueAction(std::move(action1));
  EXPECT_TRUE(action1_called);

  test_layout->SetLayout(layout2());

  // Invalidating the layout forces a recalculation, which starts the animation.
  view()->InvalidateLayout();

  // Since the animation is running, this action is queued for later.
  layout()->RunOrQueueAction(std::move(action2));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_TRUE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance partially.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_TRUE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance to completion.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_TRUE(action1_called);
  EXPECT_FALSE(action2_called);

  // Final layout clears the |is_animating| state because the views are now in
  // their final configuration.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(action1_called);
  EXPECT_TRUE(action2_called);
}

TEST_F(AnimatingLayoutManagerSteppingTest, ZOrder_UnchangedWhenNotAnimating) {
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());
}

TEST_F(AnimatingLayoutManagerSteppingTest, ZOrder_UnchangedWhenNotFading) {
  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  // Start the animation.
  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  SizeAndLayout();

  // At this point animation should have started, but not proceeded.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());

  // Advance partially.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());

  // Advance to end.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());
}

TEST_F(AnimatingLayoutManagerSteppingTest, ZOrder_FadingOutViewMovedToBack) {
  const ProposedLayout starting_layout{{10, 10},
                                       {{child(0), true, {1, 1, 2, 2}},
                                        {child(1), true, {3, 3, 2, 2}},
                                        {child(2), true, {7, 7, 2, 2}}}};

  const ProposedLayout ending_layout{{8, 8},
                                     {{child(0), true, {1, 1, 2, 2}},
                                      {child(1), false},
                                      {child(2), true, {5, 5, 2, 2}}}};

  const std::vector<View*> expected_order{child(1), child(0), child(2)};

  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(starting_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  // Start the animation.
  test_layout->SetLayout(ending_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  EXPECT_EQ(expected_order, view()->GetChildrenInZOrder());

  // Advance partially.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(expected_order, view()->GetChildrenInZOrder());

  // Advance to end (restores Z order).
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());
}

TEST_F(AnimatingLayoutManagerSteppingTest, ZOrder_FadingInViewMovedToBack) {
  const ProposedLayout starting_layout{{8, 8},
                                       {{child(0), true, {1, 1, 2, 2}},
                                        {child(1), false},
                                        {child(2), true, {5, 5, 2, 2}}}};

  const ProposedLayout ending_layout{{10, 10},
                                     {{child(0), true, {1, 1, 2, 2}},
                                      {child(1), true, {3, 3, 2, 2}},
                                      {child(2), true, {7, 7, 2, 2}}}};

  const std::vector<View*> expected_order{child(1), child(0), child(2)};

  layout()->SetShouldAnimateBounds(true);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(starting_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  // Start the animation.
  test_layout->SetLayout(ending_layout);
  view()->InvalidateLayout();
  SizeAndLayout();
  EXPECT_EQ(expected_order, view()->GetChildrenInZOrder());

  // Advance partially.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(expected_order, view()->GetChildrenInZOrder());

  // Advance to end (restores Z order).
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());
}

TEST_F(AnimatingLayoutManagerSteppingTest, ConstrainedSpace_StopsAnimation) {
  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  SizeAndLayout();

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  // Layout 2 is 200 across. Halfway is 150. Getting less should halt the
  // animation. Note that calling SetSize() should result in a Layout() call.
  view()->SetSize({140, 200});
  EXPECT_FALSE(layout()->is_animating());
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       ConstrainedSpace_TriggersDelayedAction) {
  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  SizeAndLayout();

  bool action_called = false;
  auto action = base::BindOnce([](bool* var) { *var = true; }, &action_called);
  layout()->QueueDelayedAction(std::move(action));

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  // Layout 2 is 200 across. Halfway is 150. Getting less should halt the
  // animation. Note that calling SetSize() should result in a Layout() call.
  view()->SetSize({140, 200});
  EXPECT_TRUE(action_called);
}

TEST_F(AnimatingLayoutManagerSteppingTest,
       ConstrainedSpace_SubsequentAnimation) {
  layout()->SetShouldAnimateBounds(true);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  SizeAndLayout();

  // Advance the animation.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(500));
  // Layout 2 is 200 across. Halfway is 150. Getting less should halt the
  // animation. Note that calling SetSize() should result in a Layout() call.
  view()->SetSize({140, 200});

  // This should attempt to restart the animation.
  view()->InvalidateLayout();
  EXPECT_TRUE(layout()->is_animating());

  // And this should halt it again.
  animation_api()->IncrementTime(base::TimeDelta::FromMilliseconds(200));
  view()->Layout();
  EXPECT_FALSE(layout()->is_animating());
}

namespace {

constexpr base::TimeDelta kMinimumAnimationTime =
    base::TimeDelta::FromMilliseconds(50);

// Layout manager which immediately lays out its child views when it is
// invalidated.
class ImmediateLayoutManager : public LayoutManager {
 public:
  ImmediateLayoutManager(bool use_preferred_size,
                         const SizeBounds& size_bounds = SizeBounds())
      : use_preferred_size_(use_preferred_size), size_bounds_(size_bounds) {
    DCHECK(use_preferred_size_ || size_bounds == SizeBounds());
  }

  // LayoutManager:

  void InvalidateLayout() override { Layout(host_); }

  gfx::Size GetPreferredSize(const View* view) const override {
    return gfx::Size();
  }

  void Layout(View* view) override {
    EXPECT_EQ(host_, view);
    for (View* child : host_->children()) {
      if (use_preferred_size_) {
        const gfx::Size preferred =
            ConstrainSizeToBounds(child->GetPreferredSize(), size_bounds_);
        if (preferred != child->size()) {
          // This implicityly lays out the child view.
          child->SetSize(preferred);
          continue;
        }
      }
      child->Layout();
    }
  }

  void Installed(View* host) override {
    DCHECK(!host_);
    host_ = host;
  }

 private:
  const bool use_preferred_size_;
  const SizeBounds size_bounds_;
  View* host_ = nullptr;
};

// Allows an AnimatingLayoutManager to be observed so that we can wait for an
// animation to complete in real time. Call WaitForAnimationToComplete() to
// pause execution until an animation (if any) is completed.
class AnimationWatcher : public AnimatingLayoutManager::Observer {
 public:
  explicit AnimationWatcher(AnimatingLayoutManager* layout_manager)
      : layout_manager_(layout_manager) {
    observer_.Add(layout_manager);
  }

  void OnLayoutIsAnimatingChanged(AnimatingLayoutManager*,
                                  bool is_animating) override {
    if (!is_animating && waiting_) {
      run_loop_->Quit();
      waiting_ = false;
    }
  }

  void WaitForAnimationToComplete() {
    if (!layout_manager_->is_animating())
      return;
    DCHECK(!waiting_);
    waiting_ = true;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  AnimatingLayoutManager* const layout_manager_;
  ScopedObserver<AnimatingLayoutManager, AnimatingLayoutManager::Observer>
      observer_{this};
  std::unique_ptr<base::RunLoop> run_loop_;
  bool waiting_ = false;
};

}  // anonymous namespace

// Test fixture for testing animations in realtime. Provides a parent view with
// an ImmediateLayoutManager so that when animation frames are triggered, the
// host view is laid out immediately. Animation durations are kept short to
// prevent tests from taking too long.
class AnimatingLayoutManagerRealtimeTest
    : public AnimatingLayoutManagerSteppingTest {
 public:
  void SetUp() override {
    AnimatingLayoutManagerSteppingTest::SetUp();
    root_view_ = std::make_unique<View>();
    root_view_->AddChildView(view());
    animation_watcher_ = std::make_unique<AnimationWatcher>(layout());
  }

  void TearDown() override {
    animation_watcher_.reset();
    // Don't call base version because we own the view.
  }

  bool UseContainerTestApi() const override { return false; }

  void InitRootView(const SizeBounds& bounds = SizeBounds()) {
    root_view_->SetLayoutManager(std::make_unique<ImmediateLayoutManager>(
        layout()->should_animate_bounds(), bounds));
    layout()->EnableAnimationForTesting();
  }

  AnimationWatcher* animation_watcher() { return animation_watcher_.get(); }

 private:
  std::unique_ptr<View> root_view_;
  std::unique_ptr<AnimationWatcher> animation_watcher_;
};

TEST_F(AnimatingLayoutManagerRealtimeTest, TestAnimateSlide) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(true);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  InitRootView();

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {35, 20},
      {{child(1), true, {{5, 5}, kChildViewSize}},
       {child(2), true, {{20, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(expected_start.host_size, view()->size());
  EnsureLayout(expected_start);

  view()->RemoveChildView(child(0));
  EXPECT_TRUE(layout()->is_animating());
  delete child(0);

  animation_watcher()->WaitForAnimationToComplete();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(expected_end.host_size, view()->size());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerRealtimeTest, TestAnimateStretch) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetShouldAnimateBounds(false);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded));
  InitRootView();

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  const ProposedLayout expected_end{
      {50, 20},
      {{child(1), true, {{5, 5}, {25, kChildViewSize.height()}}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  view()->RemoveChildView(child(0));
  EXPECT_TRUE(layout()->is_animating());
  delete child(0);
  animation_watcher()->WaitForAnimationToComplete();

  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerRealtimeTest, TestConstrainedSpaceStopsAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  static const SizeBounds kSizeBounds(45, base::nullopt);
  layout()->SetShouldAnimateBounds(true);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  InitRootView(kSizeBounds);
  child(0)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kScaleToZero,
                                              MaximumFlexSizeRule::kPreferred));
  child(0)->SetVisible(false);
  view()->InvalidateLayout();

  const ProposedLayout starting_layout{
      {35, 20},
      {{child(1), true, {{5, 5}, kChildViewSize}},
       {child(2), true, {{20, 5}, kChildViewSize}}}};

  const ProposedLayout ending_layout{
      {45, 20},
      {{child(0), true, {{5, 5}, {5, 10}}},
       {child(1), true, {{15, 5}, kChildViewSize}},
       {child(2), true, {{30, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(starting_layout.host_size, view()->size());
  EnsureLayout(starting_layout);

  child(0)->SetVisible(true);
  EXPECT_TRUE(layout()->is_animating());

  animation_watcher()->WaitForAnimationToComplete();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(ending_layout.host_size, view()->size());
  EnsureLayout(ending_layout);
}

TEST_F(AnimatingLayoutManagerRealtimeTest,
       TestConstrainedSpaceRestartedAnimationStops) {
  constexpr gfx::Insets kChildMargins(5);
  static const SizeBounds kSizeBounds(45, base::nullopt);
  layout()->SetShouldAnimateBounds(true);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  InitRootView(kSizeBounds);
  child(0)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kScaleToZero,
                                              MaximumFlexSizeRule::kPreferred));
  child(0)->SetVisible(false);
  view()->InvalidateLayout();

  const ProposedLayout starting_layout{
      {35, 20},
      {{child(1), true, {{5, 5}, kChildViewSize}},
       {child(2), true, {{20, 5}, kChildViewSize}}}};

  const ProposedLayout ending_layout{
      {45, 20},
      {{child(0), true, {{5, 5}, {5, 10}}},
       {child(1), true, {{15, 5}, kChildViewSize}},
       {child(2), true, {{30, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(starting_layout.host_size, view()->size());
  EnsureLayout(starting_layout);

  // This should cause an animation that aborts when it hits the size bound.
  child(0)->SetVisible(true);
  animation_watcher()->WaitForAnimationToComplete();

  // Invalidating the host causes an additional layout, but animation will stop
  // immediately.
  view()->InvalidateLayout();
  EXPECT_TRUE(layout()->is_animating());
  animation_watcher()->WaitForAnimationToComplete();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(ending_layout.host_size, view()->size());
  EnsureLayout(ending_layout);
}

TEST_F(AnimatingLayoutManagerRealtimeTest,
       TestConstrainedSpaceRestartedAnimationSucceeds) {
  constexpr gfx::Insets kChildMargins(5);
  static const SizeBounds kSizeBounds(45, base::nullopt);
  layout()->SetShouldAnimateBounds(true);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  InitRootView(kSizeBounds);
  child(0)->SetProperty(kFlexBehaviorKey, FlexSpecification::ForSizeRule(
                                              MinimumFlexSizeRule::kScaleToZero,
                                              MaximumFlexSizeRule::kPreferred));
  child(0)->SetVisible(false);
  view()->InvalidateLayout();

  const ProposedLayout starting_layout{
      {35, 20},
      {{child(1), true, {{5, 5}, kChildViewSize}},
       {child(2), true, {{20, 5}, kChildViewSize}}}};

  const ProposedLayout ending_layout{
      {45, 20},
      {{child(0), true, {{5, 5}, {5, 10}}},
       {child(1), true, {{15, 5}, kChildViewSize}},
       {child(2), true, {{30, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(starting_layout.host_size, view()->size());
  EnsureLayout(starting_layout);

  // This should cause an animation that aborts when it hits the size bound.
  child(0)->SetVisible(true);
  animation_watcher()->WaitForAnimationToComplete();

  // This will restart the animation, but since the target is smaller than the
  // available space, the animation will proceed.
  child(0)->SetVisible(false);
  view()->InvalidateLayout();
  EXPECT_TRUE(layout()->is_animating());
  animation_watcher()->WaitForAnimationToComplete();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(starting_layout.host_size, view()->size());
  EnsureLayout(starting_layout);
}

// TODO(dfried): figure out why these tests absolutely do not animate properly
// on Mac. Whatever magic makes the compositor animation runner go doesn't seem
// to want to work on Mac in non-browsertests :(
#if !defined(OS_MACOSX)

// Test fixture for testing sequences of the following four actions:
// * animating layout manager configured on host view
// * host view added to parent view
// * parent view added to widget
// * child view added to host view
//
// The result will either be an animation or no animation, but both will have
// the same final layout. We will not test all possible sequences, but a
// representative sample based on what sequences of actions we are (a) likely to
// see and (b) hit most possible code paths.
class AnimatingLayoutManagerSequenceTest : public ViewsTestBase {
 public:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_.reset(new Widget());
    auto params = CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = {0, 0, 500, 500};
    widget_->Init(std::move(params));

    parent_view_ptr_ = std::make_unique<View>();
    parent_view_ptr_->SetLayoutManager(
        std::make_unique<ImmediateLayoutManager>(true));
    parent_view_ = parent_view_ptr_.get();

    layout_view_ptr_ = std::make_unique<View>();
    layout_view_ = layout_view_ptr_.get();
  }

  void TearDown() override {
    // Do before rest of tear down.
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void ConfigureLayoutView() {
    layout_manager_ = layout_view_->SetLayoutManager(
        std::make_unique<AnimatingLayoutManager>());
    layout_manager_->SetTweenType(gfx::Tween::Type::LINEAR);
    layout_manager_->SetAnimationDuration(kMinimumAnimationTime);
    auto* const flex_layout =
        layout_manager_->SetTargetLayoutManager(std::make_unique<FlexLayout>());
    flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
    flex_layout->SetCollapseMargins(true);
    flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
    flex_layout->SetDefault(kMarginsKey, gfx::Insets(5));
    layout_manager_->SetShouldAnimateBounds(true);
  }

  void AddViewToParent() {
    parent_view_->AddChildView(std::move(layout_view_ptr_));
  }

  void AddParentToWidget() {
    widget_->GetRootView()->AddChildView(std::move(parent_view_ptr_));
  }

  void AddChildToLayoutView() {
    auto child_view_ptr = std::make_unique<View>();
    child_view_ptr->SetPreferredSize(gfx::Size(10, 10));
    child_view_ = layout_view_->AddChildView(std::move(child_view_ptr));
  }

  void ExpectResetToLayout() {
    EXPECT_FALSE(layout_manager_->is_animating());
    EXPECT_EQ(gfx::Size(20, 20), layout_view_->size());
    EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child_view_->bounds());
  }

  void ExpectAnimateToLayout() {
    EXPECT_TRUE(layout_manager_->is_animating());
    AnimationWatcher animation_watcher(layout_manager_);
    animation_watcher.WaitForAnimationToComplete();
    EXPECT_EQ(gfx::Size(20, 20), layout_view_->size());
    EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child_view_->bounds());
  }

 private:
  struct WidgetCloser {
    inline void operator()(Widget* widget) const { widget->CloseNow(); }
  };

  using WidgetAutoclosePtr = std::unique_ptr<Widget, WidgetCloser>;

  AnimatingLayoutManager* layout_manager_ = nullptr;
  View* child_view_ = nullptr;
  View* parent_view_ = nullptr;
  View* layout_view_ = nullptr;
  std::unique_ptr<View> parent_view_ptr_;
  std::unique_ptr<View> layout_view_ptr_;
  WidgetAutoclosePtr widget_;
};

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddChild_AddToParent_Configure_AddToWidget) {
  AddChildToLayoutView();
  AddViewToParent();
  ConfigureLayoutView();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddChild_Configure_AddToParent_AddToWidget) {
  AddChildToLayoutView();
  ConfigureLayoutView();
  AddViewToParent();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       Configure_AddChild_AddToParent_AddToWidget) {
  ConfigureLayoutView();
  AddChildToLayoutView();
  AddViewToParent();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       Configure_AddToParent_AddChild_AddToWidget) {
  ConfigureLayoutView();
  AddViewToParent();
  AddChildToLayoutView();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddToParent_Configure_AddChild_AddToWidget) {
  AddViewToParent();
  ConfigureLayoutView();
  AddChildToLayoutView();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddToParent_AddChild_Configure_AddToWidget) {
  AddViewToParent();
  AddChildToLayoutView();
  ConfigureLayoutView();
  AddParentToWidget();

  ExpectResetToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddToWidget_AddToParent_Configure_AddChild) {
  AddParentToWidget();
  AddViewToParent();
  ConfigureLayoutView();
  AddChildToLayoutView();

  ExpectAnimateToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddToParent_AddToWidget_Configure_AddChild) {
  AddViewToParent();
  AddParentToWidget();
  ConfigureLayoutView();
  AddChildToLayoutView();

  ExpectAnimateToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       Configure_AddToParent_AddToWidget_AddChild) {
  ConfigureLayoutView();
  AddViewToParent();
  AddParentToWidget();
  AddChildToLayoutView();

  ExpectAnimateToLayout();
}

TEST_F(AnimatingLayoutManagerSequenceTest,
       AddToWidget_AddToParent_AddChild_Configure) {
  AddParentToWidget();
  AddViewToParent();
  AddChildToLayoutView();
  ConfigureLayoutView();

  ExpectResetToLayout();
}

#endif  // !defined(OS_MACOSX)

}  // namespace views

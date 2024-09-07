// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/animating_layout_manager.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

constexpr gfx::Size kChildViewSize{10, 10};

// Returns a size which is the intersection of |size| and the constraints
// provided by |bounds|, if any.
gfx::Size ConstrainSizeToBounds(const gfx::Size& size,
                                const SizeBounds& bounds) {
  return gfx::Size(bounds.width().min_of(size.width()),
                   bounds.height().min_of(size.height()));
}

// View that allows directly setting minimum size.
class TestView : public View {
  METADATA_HEADER(TestView, View)

 public:
  using View::View;
  ~TestView() override = default;

  void set_preferred_size(gfx::Size preferred_size) {
    preferred_size_ = preferred_size;
  }

  void SetMinimumSize(gfx::Size minimum_size) { minimum_size_ = minimum_size; }
  gfx::Size GetMinimumSize() const override {
    return minimum_size_ ? *minimum_size_ : View::GetMinimumSize();
  }

  void SetFixArea(bool fix_area) { fix_area_ = fix_area; }
  bool fix_area() const { return fix_area_; }

  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override {
    gfx::Size preferred_size =
        preferred_size_ ? preferred_size_.value()
                        : View::CalculatePreferredSize(available_size);
    if (fix_area_) {
      const int min_width = minimum_size_ ? minimum_size_->width() : 0;
      const int width = std::max(
          min_width, available_size.width().value_or(preferred_size.width()));
      const int height =
          preferred_size.height() * preferred_size.width() / std::max(1, width);
      return gfx::Size(width, height);
    }

    return preferred_size;
  }

 private:
  std::optional<gfx::Size> preferred_size_;
  std::optional<gfx::Size> minimum_size_;
  bool fix_area_ = false;
};

BEGIN_METADATA(TestView)
END_METADATA

// Layout that provides a predictable target layout for an
// AnimatingLayoutManager.
class TestLayoutManager : public LayoutManagerBase {
 public:
  TestLayoutManager() = default;
  ~TestLayoutManager() override = default;

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
      if (child->GetVisible()) {
        preferred_size.SetToMax(child->GetPreferredSize({}));
      }
    }
    gfx::Rect rect(preferred_size);
    rect.Inset(-host->GetInsets());
    return rect.size();
  }

  gfx::Size GetPreferredSize(const View* host,
                             const SizeBounds& available_size) const override {
    if (host->children().empty()) {
      return gfx::Size();
    }

    gfx::Size preferred_size;
    for (View* child : host->children()) {
      if (child->GetVisible()) {
        preferred_size.SetToMax(child->GetPreferredSize({}));
      }
    }
    gfx::Rect rect(preferred_size);
    rect.Inset(-host->GetInsets());
    return rect.size();
  }
};

class AnimationEventLogger : public AnimatingLayoutManager::Observer {
 public:
  ~AnimationEventLogger() override = default;

  explicit AnimationEventLogger(AnimatingLayoutManager* layout) {
    scoped_observation_.Observe(layout);
  }

  void OnLayoutIsAnimatingChanged(AnimatingLayoutManager* source,
                                  bool is_animating) override {
    events_.push_back(is_animating);
  }

  const std::vector<bool> events() const { return events_; }

 private:
  std::vector<bool> events_;
  base::ScopedObservation<AnimatingLayoutManager, Observer> scoped_observation_{
      this};
};

}  // anonymous namespace

// Test fixture which creates an AnimatingLayoutManager and instruments it so
// the animations can be directly controlled via gfx::AnimationContainerTestApi.
class AnimatingLayoutManagerTest : public testing::Test {
 public:
  explicit AnimatingLayoutManagerTest(bool enable_animations = true)
      : enable_animations_(enable_animations) {}
  ~AnimatingLayoutManagerTest() override = default;

  void SetUp() override {
    render_mode_lock_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        enable_animations_
            ? gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED
            : gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);

    // Don't use a unique_ptr because derived classes may want to own this view.
    view_ = new View();
    for (int i = 0; i < 3; ++i) {
      auto child = std::make_unique<TestView>();
      child->set_preferred_size(kChildViewSize);
      children_.push_back(view_->AddChildView(std::move(child)));
    }
    view_->SetLayoutManager(std::make_unique<AnimatingLayoutManager>());

    // Use linear transitions to make expected values predictable.
    layout()->SetTweenType(gfx::Tween::Type::LINEAR);
    layout()->SetAnimationDuration(base::Seconds(1));

    if (UseContainerTestApi()) {
      container_test_api_ = std::make_unique<gfx::AnimationContainerTestApi>(
          layout()->GetAnimationContainerForTesting());
    }

    // These can't be constructed statically since they depend on the child
    // views.
    layout1_ = {{100, 100},
                {{children_[0].get(), true, {5, 5, 10, 10}},
                 {children_[1].get(), false},
                 {children_[2].get(), true, {20, 20, 20, 20}}}};
    layout2_ = {{200, 200},
                {{children_[0].get(), true, {10, 20, 20, 30}},
                 {children_[1].get(), false},
                 {children_[2].get(), true, {10, 100, 10, 10}}}};
  }

  void TearDown() override {
    DestroyView();
    render_mode_lock_.reset();
  }

  const View* view() const { return view_; }
  View* view() { return view_; }
  TestView* child(size_t index) const { return children_[index]; }
  size_t num_children() const { return children_.size(); }
  AnimatingLayoutManager* layout() {
    return static_cast<AnimatingLayoutManager*>(view_->GetLayoutManager());
  }
  gfx::AnimationContainerTestApi* animation_api() {
    return container_test_api_.get();
  }
  const ProposedLayout& layout1() const { return layout1_; }
  const ProposedLayout& layout2() const { return layout2_; }

  void RunCurrentTasks() {
    base::RunLoop loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

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

  void EnsureLayout(const ProposedLayout& expected, const char* message = "") {
    for (size_t i = 0; i < expected.child_layouts.size(); ++i) {
      const auto& expected_child = expected.child_layouts[i];
      const View* const child = expected_child.child_view;
      EXPECT_EQ(view_, child->parent())
          << " view " << i << " parent differs " << message;
      EXPECT_EQ(expected_child.visible, child->GetVisible())
          << " view " << i << " visibility " << message;
      if (expected_child.visible) {
        EXPECT_EQ(expected_child.bounds, child->bounds())
            << " view " << i << " bounds " << message;
      }
    }
  }

  void DestroyView() {
    if (view_) {
      delete view_.ExtractAsDangling();
    }
  }

  void SizeAndLayout() {
    // If the layout of |view| is invalid or the size changes, this will
    // automatically lay out the view as well.
    view_->SizeToPreferredSize();
  }

  virtual bool UseContainerTestApi() const { return true; }

  static const FlexSpecification kDropOut;
  static const FlexSpecification kFlex;

 private:
  const bool enable_animations_;
  ProposedLayout layout1_;
  ProposedLayout layout2_;
  raw_ptr<View, DanglingUntriaged> view_ = nullptr;
  std::vector<raw_ptr<TestView, VectorExperimental>> children_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<gfx::AnimationContainerTestApi> container_test_api_;
  gfx::AnimationTestApi::RenderModeResetter render_mode_lock_;
};

const FlexSpecification AnimatingLayoutManagerTest::kDropOut =
    FlexSpecification(MinimumFlexSizeRule::kPreferredSnapToZero,
                      MaximumFlexSizeRule::kPreferred)
        .WithWeight(0);

const FlexSpecification AnimatingLayoutManagerTest::kFlex =
    FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                      MaximumFlexSizeRule::kUnbounded)
        .WithOrder(2);

TEST_F(AnimatingLayoutManagerTest, SetLayoutManager_NoAnimation) {
  auto test_layout = std::make_unique<TestLayoutManager>();
  test_layout->SetLayout(layout1());
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  layout()->SetTargetLayoutManager(std::move(test_layout));

  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerTest, ResetLayout_NoAnimation) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();

  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerTest, HostInvalidate_TriggersAnimation) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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

TEST_F(AnimatingLayoutManagerTest,
       HostInvalidate_AnimateBounds_AnimationProgresses) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  SizeAndLayout();

  // Advance the animation.
  animation_api()->IncrementTime(base::Milliseconds(250));
  SizeAndLayout();
  ProposedLayout expected = ProposedLayoutBetween(0.25, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance again.
  animation_api()->IncrementTime(base::Milliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  expected = layout2();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);
}

TEST_F(AnimatingLayoutManagerTest, HostInvalidate_NoAnimateBounds_NoAnimation) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  SizeAndLayout();
  layout()->ResetLayout();
  test::RunScheduledLayout(view());

  // First layout. Should not be animating.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Because the desired layout did not change, there is no animation.
  view()->InvalidateLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerTest,
       HostInvalidate_NoAnimateBounds_NewLayoutTriggersAnimation) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  SizeAndLayout();
  layout()->ResetLayout();
  test::RunScheduledLayout(view());

  // First layout. Should not be animating.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Switching to the new layout without changing size will lead to an
  // animation.
  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerTest,
       HostInvalidate_NoAnimateBounds_AnimationProgresses) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  SizeAndLayout();
  layout()->ResetLayout();
  test::RunScheduledLayout(view());

  // First layout. Should not be animating.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());

  // Switching to the new layout without changing size will lead to an
  // animation.
  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  // No change to the layout yet.
  EnsureLayout(layout1());

  // Advance the animation.
  animation_api()->IncrementTime(base::Milliseconds(250));
  test::RunScheduledLayout(view());
  ProposedLayout expected = ProposedLayoutBetween(0.25, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected);

  // Advance again.
  animation_api()->IncrementTime(base::Milliseconds(250));
  test::RunScheduledLayout(view());
  expected = ProposedLayoutBetween(0.5, layout1(), layout2());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  expected = layout2();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected);
}

TEST_F(AnimatingLayoutManagerTest, FadeInOutMode_MiddleView_ScaleFromZero) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(250));
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
  animation_api()->IncrementTime(base::Milliseconds(250));
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
  animation_api()->IncrementTime(base::Milliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.75, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // zero in size so it will disappear.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(250));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(final_layout.host_size, view()->size());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest, FadeInOutMode_MiddleView_ScaleFromMinimum) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(250));
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
  animation_api()->IncrementTime(base::Milliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // its minimum size so it will disappear.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(final_layout.host_size, view()->size());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest, FadeInOutMode_LeadingView_ScaleFromMinimum) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(250));
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
  animation_api()->IncrementTime(base::Milliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // its minimum size so it will disappear.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(final_layout.host_size, view()->size());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest,
       FadeInOutMode_TrailingView_ScaleFromMinimum_FadeIn) {
  const ProposedLayout initial_layout{{35, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), false}}};

  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // its minimum size so it will not be visible.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance the animation.
  animation_api()->IncrementTime(base::Milliseconds(250));
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
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(final_layout.host_size, view()->size());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest,
       FadeInOutMode_TrailingView_ScaleFromMinimum) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(250));
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
  animation_api()->IncrementTime(base::Milliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // its minimum size so it will disappear.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected.host_size, view()->size());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(final_layout.host_size, view()->size());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest,
       FadeInOutMode_TrailingView_ScaleFromMinimum_Vertical) {
  const ProposedLayout initial_layout{{20, 50},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {5, 20, 10, 10}},
                                       {child(2), true, {5, 35, 10, 10}}}};
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(250));
  test::RunScheduledLayout(view());
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
  animation_api()->IncrementTime(base::Milliseconds(250));
  SizeAndLayout();
  expected = ProposedLayoutBetween(0.5, initial_layout, final_layout);
  // At this point the layout is still animating but the middle view is below
  // its minimum size so it will disappear.
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected);

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest,
       FadeInOutMode_Hide_HidesViewDuringAnimation) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetDefaultFadeMode(AnimatingLayoutManager::FadeInOutMode::kHide);
  layout()->SetOrientation(LayoutOrientation::kVertical);
  FlexLayout* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetDefault(kMarginsKey, gfx::Insets(5))
      .SetCollapseMargins(true)
      .SetOrientation(LayoutOrientation::kVertical)
      .SetDefault(kFlexBehaviorKey, kDropOut);
  view()->SetSize({20, 35});
  layout()->ResetLayout();
  test::RunScheduledLayout(view());

  // Sanity check...
  const ProposedLayout initial_layout{{20, 50},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {5, 20, 10, 10}},
                                       {child(2), false}}};
  EnsureLayout(initial_layout);

  // Hide middle view.
  layout()->FadeOut(child(1));
  EXPECT_TRUE(layout()->is_animating());

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());

  const ProposedLayout middle_layout{
      {20, 35},
      {{child(0), true, {5, 5, 10, 10}}, {child(1), false}, {child(2), false}}};
  EnsureLayout(middle_layout);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  const ProposedLayout final_layout{{20, 35},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), false},
                                     {child(2), true, {5, 20, 10, 10}}}};
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest,
       FadeInOutMode_Hide_HidesViewDuringAnimation_OneFrame) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetDefaultFadeMode(AnimatingLayoutManager::FadeInOutMode::kHide);
  layout()->SetOrientation(LayoutOrientation::kVertical);
  FlexLayout* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetDefault(kMarginsKey, gfx::Insets(5))
      .SetCollapseMargins(true)
      .SetOrientation(LayoutOrientation::kVertical)
      .SetDefault(kFlexBehaviorKey, kDropOut);
  view()->SetSize({20, 35});
  layout()->ResetLayout();
  test::RunScheduledLayout(view());

  // Sanity check...
  const ProposedLayout initial_layout{{20, 50},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {5, 20, 10, 10}},
                                       {child(2), false}}};
  EnsureLayout(initial_layout);

  // Hide middle view.
  layout()->FadeOut(child(1));
  EXPECT_TRUE(layout()->is_animating());

  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(view());

  const ProposedLayout final_layout{{20, 35},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), false},
                                     {child(2), true, {5, 20, 10, 10}}}};
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest,
       FadeInOutMode_Hide_AnimationResetDuringHide) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetDefaultFadeMode(AnimatingLayoutManager::FadeInOutMode::kHide);
  layout()->SetOrientation(LayoutOrientation::kVertical);
  FlexLayout* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetDefault(kMarginsKey, gfx::Insets(5))
      .SetCollapseMargins(true)
      .SetOrientation(LayoutOrientation::kVertical)
      .SetDefault(kFlexBehaviorKey, kDropOut);
  view()->SetSize({20, 35});
  layout()->ResetLayout();
  test::RunScheduledLayout(view());

  // Sanity check...
  const ProposedLayout initial_layout{{20, 50},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {5, 20, 10, 10}},
                                       {child(2), false}}};
  EnsureLayout(initial_layout);

  // Hide middle view.
  layout()->FadeOut(child(1));
  layout()->ResetLayout();
  test::RunScheduledLayout(view());

  const ProposedLayout final_layout{{20, 35},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), false},
                                     {child(2), true, {5, 20, 10, 10}}}};
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest, FadeInOutMode_SlideFromLeading_LastView) {
  const ProposedLayout initial_layout{{35, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), false}}};
  const ProposedLayout final_layout{{50, 20},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), true, {20, 5, 10, 10}},
                                     {child(2), true, {35, 5, 10, 10}}}};

  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(200));
  SizeAndLayout();
  expected_layout.host_size = {38, 20};
  expected_layout.child_layouts[2].bounds = {23, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation 60%.
  animation_api()->IncrementTime(base::Milliseconds(600));
  SizeAndLayout();
  expected_layout.host_size = {47, 20};
  expected_layout.child_layouts[2].bounds = {32, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation to completion.
  animation_api()->IncrementTime(base::Milliseconds(200));
  SizeAndLayout();
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest, FadeInOutMode_SlideFromLeading_Vertical) {
  const ProposedLayout initial_layout{{20, 35},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {5, 20, 10, 10}},
                                       {child(2), false}}};
  const ProposedLayout final_layout{{20, 50},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), true, {5, 20, 10, 10}},
                                     {child(2), true, {5, 35, 10, 10}}}};

  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(200));
  SizeAndLayout();
  expected_layout.host_size = {20, 38};
  expected_layout.child_layouts[2].bounds = {5, 23, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation 60%.
  animation_api()->IncrementTime(base::Milliseconds(600));
  SizeAndLayout();
  expected_layout.host_size = {20, 47};
  expected_layout.child_layouts[2].bounds = {5, 32, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation to completion.
  animation_api()->IncrementTime(base::Milliseconds(200));
  SizeAndLayout();
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest, FadeInOutMode_SlideFromLeading_MiddleView) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 5, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};

  const ProposedLayout final_layout{{35, 20},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), false},
                                     {child(2), true, {20, 5, 10, 10}}}};

  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(200));
  SizeAndLayout();
  expected_layout.host_size = {47, 20};
  expected_layout.child_layouts[1].bounds = {18, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {32, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation 60%.
  animation_api()->IncrementTime(base::Milliseconds(600));
  SizeAndLayout();
  expected_layout.host_size = {38, 20};
  expected_layout.child_layouts[1].bounds = {12, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {23, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation to completion.
  animation_api()->IncrementTime(base::Milliseconds(200));
  SizeAndLayout();
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest,
       FadeInOutMode_SlideFromLeading_LeadingView_SlidesFromTrailing) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 5, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};

  const ProposedLayout final_layout{{35, 20},
                                    {{child(0), false},
                                     {child(1), true, {5, 5, 5, 10}},
                                     {child(2), true, {20, 5, 10, 10}}}};

  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(200));
  SizeAndLayout();
  expected_layout.host_size = {38, 20};
  expected_layout.child_layouts[1].bounds = {17, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {32, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation 60%.
  animation_api()->IncrementTime(base::Milliseconds(600));
  SizeAndLayout();
  expected_layout.host_size = {47, 20};
  expected_layout.child_layouts[1].bounds = {8, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {23, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation to completion.
  animation_api()->IncrementTime(base::Milliseconds(200));
  SizeAndLayout();
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest, FadeInOutMode_SlideFromTrailing_MiddleView) {
  const ProposedLayout initial_layout{{50, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 5, 10}},
                                       {child(2), true, {35, 5, 10, 10}}}};

  const ProposedLayout final_layout{{35, 20},
                                    {{child(0), true, {5, 5, 10, 10}},
                                     {child(1), false},
                                     {child(2), true, {20, 5, 10, 10}}}};

  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(200));
  SizeAndLayout();
  expected_layout.host_size = {47, 20};
  expected_layout.child_layouts[1].bounds = {20, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {32, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation 60%.
  animation_api()->IncrementTime(base::Milliseconds(600));
  SizeAndLayout();
  expected_layout.host_size = {38, 20};
  expected_layout.child_layouts[1].bounds = {20, 5, 5, 10};
  expected_layout.child_layouts[2].bounds = {23, 5, 10, 10};
  EnsureLayout(expected_layout);

  // Advance the animation to completion.
  animation_api()->IncrementTime(base::Milliseconds(200));
  SizeAndLayout();
  EnsureLayout(final_layout);
}

TEST_F(AnimatingLayoutManagerTest, FlexLayout_FadeOutOnVisibilitySet) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_start.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerTest, FlexLayout_FadeInOnVisibilitySet) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
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
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(true);

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_end.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Regression test for issues: crbug.com/1021332, crbug.com/1003500
TEST_F(AnimatingLayoutManagerTest,
       FlexLayout_AnimateOutOnDescendentVisibilitySet) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
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
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  grandchild->SetVisible(false);

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_start.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Regression test for issues: crbug.com/1021332, crbug.com/1003500
TEST_F(AnimatingLayoutManagerTest,
       FlexLayout_AnimateInOnDescendentVisibilitySet) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
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
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  grandchild->SetVisible(true);

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_end.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Regression test for crbug.com/1037625: crash in SetViewVisibility() (1/2)
TEST_F(AnimatingLayoutManagerTest, FlexLayout_RemoveFadingViewDoesNotCrash) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);

  const ProposedLayout expected_start{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  layout()->FadeOut(child(1));

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());

  View* const child1 = child(1);
  view()->RemoveChildView(child1);
  delete child1;

  animation_api()->IncrementTime(base::Milliseconds(250));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());

  animation_api()->IncrementTime(base::Milliseconds(250));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
}

// Regression test for crbug.com/1037625: crash in SetViewVisibility() (2/2)
TEST_F(AnimatingLayoutManagerTest, FlexLayout_RemoveShowingViewDoesNotCrash) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  child(1)->SetVisible(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);

  // Set up the initial state of the host view and children.
  SizeAndLayout();
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());

  layout()->FadeIn(child(1));

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());

  View* const child1 = child(1);
  view()->RemoveChildView(child1);
  delete child1;

  animation_api()->IncrementTime(base::Milliseconds(250));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());

  animation_api()->IncrementTime(base::Milliseconds(250));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
}

// Regression test for crbug.com/1037947 (1/2)
TEST_F(AnimatingLayoutManagerTest, FlexLayout_DoubleSlide) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kSlideFromTrailingEdge);
  child(1)->SetVisible(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kCenter);

  layout()->ResetLayout();
  SizeAndLayout();

  const ProposedLayout expected_start{
      {20, 10},
      {{child(0), true, {{0, 0}, kChildViewSize}},
       {child(1), false},
       {child(2), true, {{10, 0}, kChildViewSize}}}};
  EnsureLayout(expected_start, "before visibility changes");

  child(0)->SetVisible(false);
  child(1)->SetVisible(true);

  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());

  const ProposedLayout expected_middle{
      {20, 10},
      {{child(0), true, {{5, 0}, kChildViewSize}},
       {child(1), true, {{5, 0}, kChildViewSize}},
       {child(2), true, {{10, 0}, kChildViewSize}}}};
  EnsureLayout(expected_middle, "during first slide");

  // Complete the layout.
  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());

  EXPECT_FALSE(layout()->is_animating());
  const ProposedLayout expected_end{
      {20, 10},
      {{child(0), false},
       {child(1), true, {{0, 0}, kChildViewSize}},
       {child(2), true, {{10, 0}, kChildViewSize}}}};
  EnsureLayout(expected_end, "after first slide");

  // Reverse the layout.
  child(0)->SetVisible(true);
  child(1)->SetVisible(false);

  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_middle, "during second slide");

  // Complete the layout.
  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start, "after second slide");
}

// Regression test for crbug.com/1037947 (2/2) - Tests a case during sliding
// where if an animation is reversed after a fading-in and fading-out views have
// exchanged relative positions in the layout, the new fading-out view will
// slide behind the wrong view.
//
// Incorrect behavior (C slides behind B):
// [A]    [B]
// [A]C] [B]
// [A][C]B]
// [A][B[C]   <--- animation is reversed here
// [A] [B]C]
// [A]    [B]
//
// Correct behavior (C slides behind A):
// [A]    [B]
// [A]C] [B]
// [A][C]B]
// [A][B[C]   <--- animation is reversed here
// [A][C[B]
// [A]C] [B]
// [A]    [B]
//
TEST_F(AnimatingLayoutManagerTest, FlexLayout_RedirectAfterExchangePlaces) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kSlideFromLeadingEdge);
  child(2)->SetVisible(false);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, gfx::Insets(50));

  layout()->ResetLayout();
  SizeAndLayout();

  // Initial layout change: show 2, hide 1.
  layout()->FadeOut(child(1));
  layout()->FadeIn(child(2));

  // Advance the layout most of the way.
  animation_api()->IncrementTime(base::Milliseconds(750));
  test::RunScheduledLayout(view());

  // Verify that the two views are visible and that they have passed each other.
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_GT(child(2)->bounds().x(), child(1)->bounds().right());

  // Save the bounds of both views to verify that child(1) moves right while
  // child(2) moves left.
  const gfx::Rect old_child1_bounds = child(1)->bounds();
  const gfx::Rect old_child2_bounds = child(2)->bounds();

  // Reverse the layout direction.
  layout()->FadeIn(child(1));
  layout()->FadeOut(child(2));

  // Advance the layout most of the way.
  animation_api()->IncrementTime(base::Milliseconds(150));
  test::RunScheduledLayout(view());

  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_GT(child(1)->x(), old_child1_bounds.x());
  EXPECT_LT(child(2)->x(), old_child2_bounds.x());
}

// Regression test for issue crbug.com/1040173 (1/2):
// PostOrQueueAction does not delay an action after FadeIn is called.
TEST_F(AnimatingLayoutManagerTest,
       FlexLayout_PostDelayedActionAfterFadeIn_AnimateNewViewIn) {
  child(0)->SetVisible(false);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);

  layout()->ResetLayout();
  SizeAndLayout();

  bool action_run = false;

  layout()->FadeIn(child(0));
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action_run));
  // No tasks should be posted, we're still animating.
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action_run);

  // Advance the animation to the end.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  SizeAndLayout();
  // We should be done and tasks will post.
  EXPECT_FALSE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_TRUE(action_run);
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
}

// Regression test for issue crbug.com/1040173 (2/2):
// PostOrQueueAction does not delay an action after FadeIn is called.
TEST_F(AnimatingLayoutManagerTest,
       FlexLayout_PostDelayedActionAfterFadeIn_SwapTwoViews) {
  child(0)->SetVisible(false);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);

  view()->SetSize({20, 10});
  layout()->ResetLayout();
  test::RunScheduledLayout(view());

  bool action_run = false;

  layout()->FadeIn(child(0));
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action_run));
  // No tasks should be posted, we're still animating.
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action_run);

  // Advance the animation to the end.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(view());
  // We should be done and tasks will post.
  EXPECT_FALSE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_TRUE(action_run);
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());
}

// Regression test for issues crbug.com/1040618 and crbug.com/1040676:
// Views hidden due to layout constraints were not shown after a flex rule
// change and FadeIn() was called.
TEST_F(AnimatingLayoutManagerTest,
       FlexLayout_PostDelayedActionAfterFadeIn_FadeInHiddenView) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);

  view()->SetSize({20, 10});
  layout()->ResetLayout();
  test::RunScheduledLayout(view());

  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  bool action_run = false;

  // This prevents the view from dropping out.
  child(2)->SetProperty(kFlexBehaviorKey, FlexSpecification());
  // The view is already potentially visible; this line should still trigger a
  // recalculation and a new animation.
  layout()->FadeIn(child(2));
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action_run));
  // No tasks should be posted, we're still animating.
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action_run);

  // Advance the animation to the end.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(view());
  // We should be done and tasks will post.
  EXPECT_FALSE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_TRUE(action_run);
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
}

// Regression test for issue 1046393 (crash/use-after-free when removing view
// during animation).
TEST_F(AnimatingLayoutManagerTest, RemoveDuringAnimationDoesntCrash) {
  const ProposedLayout initial_layout{{35, 20},
                                      {{child(0), true, {5, 5, 10, 10}},
                                       {child(1), true, {20, 5, 10, 10}},
                                       {child(2), false}}};
  const ProposedLayout final_layout{
      {20, 20},
      {{child(0), true, {5, 5, 10, 10}}, {child(1), false}, {child(2), false}}};
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kSlideFromLeadingEdge);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(initial_layout);
  layout()->ResetLayout();
  SizeAndLayout();

  // Hide the second view.
  test_layout->SetLayout(final_layout);

  // Advance the animation. Second view should still be visible, third view
  // should be hidden.
  animation_api()->IncrementTime(base::Milliseconds(500));

  // Remove third view.
  View* const child2 = child(2);
  view()->RemoveChildView(child2);
  delete child2;

  // There is still layout data for the third view; the target hasn't changed;
  // it's critical that during the removal the current layout has had the third
  // view excised or there will be a DCHECK() here.
  test::RunScheduledLayout(view());
}

TEST_F(AnimatingLayoutManagerTest, FlexLayout_FadeInOnAdded) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
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
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  view()->AddChildViewAt(child(0), 0);

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(after_add);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected = ProposedLayoutBetween(0.5, after_add, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_end.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerTest, FlexLayout_FadeIn) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
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
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  layout()->FadeIn(child(0));

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_end.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerTest, FlexLayout_FadeOut) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  layout()->FadeOut(child(0));

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  expected.child_layouts[0].visible = true;
  expected.child_layouts[0].bounds = expected_start.child_layouts[0].bounds;
  expected.child_layouts[0].bounds.set_width(
      expected.child_layouts[1].bounds.x() - 10);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerTest, FlexLayout_FadeOut_NoCrashOnRemove) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  View* const removed = child(0);
  view()->RemoveChildView(removed);
  delete removed;

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(after_remove);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, after_remove, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

TEST_F(AnimatingLayoutManagerTest, FlexLayout_FadeOut_IgnoreChildView) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetOrientation(LayoutOrientation::kHorizontal);
  layout()->SetDefaultFadeMode(
      AnimatingLayoutManager::FadeInOutMode::kScaleFromZero);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetProperty(kViewIgnoredByLayoutKey, true);

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  EnsureLayout(expected_start);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Test that when one view can flex to fill the space yielded by another view
// which is hidden, and that such a layout change triggers animation.
TEST_F(AnimatingLayoutManagerTest, FlexLayout_SlideAfterViewHidden) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.0, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  expected = ProposedLayoutBetween(0.5, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Test that when one view can flex to fill the space yielded by another view
// which is removed, and that such a layout change triggers animation.
TEST_F(AnimatingLayoutManagerTest, FlexLayout_SlideAfterViewRemoved) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  view()->RemoveChildView(child(0));
  delete child(0);

  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.0, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  expected = ProposedLayoutBetween(0.5, expected_start, expected_end);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end);
}

// Test that when an animation starts and then the target changes mid-stream,
// the animation redirects.
TEST_F(AnimatingLayoutManagerTest, FlexLayout_RedirectAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.5, expected_start, expected_end1);
  EnsureLayout(expected);

  child(2)->SetVisible(false);

  animation_api()->IncrementTime(base::Milliseconds(250));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  expected = ProposedLayoutBetween(0.5, expected, expected_end2);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(250));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end2);
}

// Test that when an animation starts and then the target changes near the end
// of the animation, the animation resets.
TEST_F(AnimatingLayoutManagerTest, FlexLayout_ResetAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  layout()->ResetLayout();
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_start);

  child(0)->SetVisible(false);

  animation_api()->IncrementTime(base::Milliseconds(900));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  ProposedLayout expected =
      ProposedLayoutBetween(0.9, expected_start, expected_end1);
  EnsureLayout(expected);

  child(2)->SetVisible(false);
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  expected = ProposedLayoutBetween(0.0, expected, expected_end2);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_TRUE(layout()->is_animating());
  expected = ProposedLayoutBetween(0.5, expected, expected_end2);
  EnsureLayout(expected);

  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_end2);
}

TEST_F(AnimatingLayoutManagerTest, TestEvents) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  EXPECT_FALSE(layout()->is_animating());
  AnimationEventLogger logger(layout());
  test_layout->SetLayout(layout2());

  // Invalidating the layout forces a recalculation, which starts the animation.
  const std::vector<bool> expected1{true};
  view()->InvalidateLayout();
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected1, logger.events());

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(expected1, logger.events());

  // Final layout clears the |is_animating| state because the views are now in
  // their final configuration.
  const std::vector<bool> expected2{true, false};
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(expected2, logger.events());
}

TEST_F(AnimatingLayoutManagerTest, PostOrQueueAction) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  EXPECT_FALSE(layout()->is_animating());
  test_layout->SetLayout(layout2());

  bool action1_called = false;
  bool action2_called = false;

  // Invalidating the layout forces a recalculation, which starts the animation.
  view()->InvalidateLayout();
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action1_called));
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action2_called));
  EXPECT_TRUE(layout()->is_animating());

  // No tasks should be posted, we're still animating.
  RunCurrentTasks();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance partially.
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Final layout clears the |is_animating| state because the views are now in
  // their final configuration.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  // The actions should now have been posted, make sure they run.
  RunCurrentTasks();
  EXPECT_TRUE(action1_called);
  EXPECT_TRUE(action2_called);
}

TEST_F(AnimatingLayoutManagerTest, PostOrQueueAction_ContinueAnimation) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  EXPECT_FALSE(layout()->is_animating());
  test_layout->SetLayout(layout2());

  bool action1_called = false;
  bool action2_called = false;

  // Invalidating the layout forces a recalculation, which starts the animation.
  view()->InvalidateLayout();
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action1_called));
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action2_called));
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance partially.
  animation_api()->IncrementTime(base::Milliseconds(850));
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Redirect the layout.
  test_layout->SetLayout(layout1());
  view()->InvalidateLayout();

  // Advance partially.
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Final layout clears the |is_animating| state because the views are now in
  // their final configuration.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  // The tasks should be posted, make sure they run.
  RunCurrentTasks();
  EXPECT_TRUE(action1_called);
  EXPECT_TRUE(action2_called);
}

TEST_F(AnimatingLayoutManagerTest, PostOrQueueAction_NeverFinishes) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  EXPECT_FALSE(layout()->is_animating());
  test_layout->SetLayout(layout2());

  bool action1_called = false;
  bool action2_called = false;

  // Invalidating the layout forces a recalculation, which starts the animation.
  view()->InvalidateLayout();
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action1_called));
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action2_called));
  EXPECT_TRUE(layout()->is_animating());

  // Advance partially.
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());

  // Flush the run loop to make sure no posting has happened before this point.
  RunCurrentTasks();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Destroy the view and the layout manager. This should not run delayed tasks.
  DestroyView();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);

  // Flush the run loop to make sure the tasks aren't posted either.
  RunCurrentTasks();
  EXPECT_FALSE(action1_called);
  EXPECT_FALSE(action2_called);
}

TEST_F(AnimatingLayoutManagerTest, PostOrQueueAction_MayPostImmediately) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  base::RunLoop loop1;
  base::RunLoop loop2;
  bool action1_called = false;
  bool action2_called = false;
  bool action3_called = false;

  // Since the layout is not animating yet, this action posts immediately.
  EXPECT_FALSE(layout()->is_animating());
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action1_called));
  RunCurrentTasks();
  EXPECT_TRUE(action1_called);
  EXPECT_FALSE(action2_called);

  test_layout->SetLayout(layout2());

  // Invalidating the layout forces a recalculation, which starts the animation.
  view()->InvalidateLayout();

  // Since the animation is running, this action is queued for later.
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action2_called));
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action2_called);

  // Advance partially.
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action2_called);

  // Advance to completion.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_TRUE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_FALSE(action2_called);

  // Final layout clears the |is_animating| state because the views are now in
  // their final configuration.
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  RunCurrentTasks();
  EXPECT_TRUE(action2_called);

  // Test that callbacks are not posted between a layout reset and the
  // subsequent layout, but are posted at the end of the layout.
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action3_called));
  RunCurrentTasks();
  EXPECT_FALSE(action3_called);
  SizeAndLayout();
  RunCurrentTasks();
  EXPECT_TRUE(action3_called);
}

TEST_F(AnimatingLayoutManagerTest, ZOrder_UnchangedWhenNotAnimating) {
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());
}

TEST_F(AnimatingLayoutManagerTest, ZOrder_UnchangedWhenNotFading) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());

  // Advance to end.
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());
}

TEST_F(AnimatingLayoutManagerTest, ZOrder_FadingOutViewMovedToBack) {
  const ProposedLayout starting_layout{{10, 10},
                                       {{child(0), true, {1, 1, 2, 2}},
                                        {child(1), true, {3, 3, 2, 2}},
                                        {child(2), true, {7, 7, 2, 2}}}};

  const ProposedLayout ending_layout{{8, 8},
                                     {{child(0), true, {1, 1, 2, 2}},
                                      {child(1), false},
                                      {child(2), true, {5, 5, 2, 2}}}};

  const std::vector<raw_ptr<View, VectorExperimental>> expected_order{
      child(1), child(0), child(2)};

  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(expected_order, view()->GetChildrenInZOrder());

  // Advance to end (restores Z order).
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());
}

TEST_F(AnimatingLayoutManagerTest, ZOrder_FadingInViewMovedToBack) {
  const ProposedLayout starting_layout{{8, 8},
                                       {{child(0), true, {1, 1, 2, 2}},
                                        {child(1), false},
                                        {child(2), true, {5, 5, 2, 2}}}};

  const ProposedLayout ending_layout{{10, 10},
                                     {{child(0), true, {1, 1, 2, 2}},
                                      {child(1), true, {3, 3, 2, 2}},
                                      {child(2), true, {7, 7, 2, 2}}}};

  const std::vector<raw_ptr<View, VectorExperimental>> expected_order{
      child(1), child(0), child(2)};

  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(expected_order, view()->GetChildrenInZOrder());

  // Advance to end (restores Z order).
  animation_api()->IncrementTime(base::Milliseconds(500));
  SizeAndLayout();
  EXPECT_EQ(view()->children(), view()->GetChildrenInZOrder());
}

TEST_F(AnimatingLayoutManagerTest, ConstrainedSpace_StopsAnimation) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  SizeAndLayout();

  // Advance the animation.
  animation_api()->IncrementTime(base::Milliseconds(500));
  // Layout 2 is 200 across. Halfway is 150. Getting less should halt the
  // animation. Note that calling SetSize() should result in a layout.
  view()->SetSize({140, 200});
  EXPECT_FALSE(layout()->is_animating());
}

TEST_F(AnimatingLayoutManagerTest, ConstrainedSpace_TriggersDelayedAction) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  SizeAndLayout();

  bool action_called = false;
  layout()->PostOrQueueAction(
      base::BindOnce([](bool* var) { *var = true; }, &action_called));
  RunCurrentTasks();
  EXPECT_FALSE(action_called);

  // Advance the animation.
  animation_api()->IncrementTime(base::Milliseconds(500));
  // Layout 2 is 200 across. Halfway is 150. Getting less should halt the
  // animation. Note that calling SetSize() should result in a layout.
  view()->SetSize({140, 200});
  // This should post the delayed actions, so make sure it actually runs.
  RunCurrentTasks();
  EXPECT_TRUE(action_called);
}

TEST_F(AnimatingLayoutManagerTest, ConstrainedSpace_SubsequentAnimation) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const test_layout =
      layout()->SetTargetLayoutManager(std::make_unique<TestLayoutManager>());
  test_layout->SetLayout(layout1());
  layout()->ResetLayout();
  SizeAndLayout();

  test_layout->SetLayout(layout2());
  view()->InvalidateLayout();
  SizeAndLayout();

  // Advance the animation.
  animation_api()->IncrementTime(base::Milliseconds(500));
  // Layout 2 is 200 across. Halfway is 150. Getting less should halt the
  // animation. Note that calling SetSize() should result in a layout.
  view()->SetSize({140, 200});

  // This should attempt to restart the animation.
  view()->InvalidateLayout();
  EXPECT_TRUE(layout()->is_animating());

  // And this should halt it again.
  animation_api()->IncrementTime(base::Milliseconds(200));
  test::RunScheduledLayout(view());
  EXPECT_FALSE(layout()->is_animating());
}

// Test which explores an Animating Layout Manager's behavior in an
// environment where rich animation is not allowed.
class AnimatingLayoutManagerNoAnimationsTest
    : public AnimatingLayoutManagerTest {
 public:
  AnimatingLayoutManagerNoAnimationsTest()
      : AnimatingLayoutManagerTest(false) {}

 protected:
  void UseFixedLayout(const views::ProposedLayout& proposed_layout) {
    TestLayoutManager* const test_layout =
        layout()->target_layout_manager()
            ? static_cast<TestLayoutManager*>(layout()->target_layout_manager())
            : layout()->SetTargetLayoutManager(
                  std::make_unique<TestLayoutManager>());
    test_layout->SetLayout(proposed_layout);
  }

  FlexLayout* UseFlexLayout() {
    return layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  }

  static const std::vector<
      std::pair<AnimatingLayoutManager::FadeInOutMode, const char*>>
      kFadeModes;
};

const std::vector<std::pair<AnimatingLayoutManager::FadeInOutMode, const char*>>
    AnimatingLayoutManagerNoAnimationsTest::kFadeModes = {
        {AnimatingLayoutManager::FadeInOutMode::kHide, "Hide"},
        {AnimatingLayoutManager::FadeInOutMode::kScaleFromZero, "Scale"},
        {AnimatingLayoutManager::FadeInOutMode::kSlideFromTrailingEdge,
         "Slide"},
};

TEST_F(AnimatingLayoutManagerNoAnimationsTest, ResetNoAnimation) {
  UseFixedLayout(layout1());
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);

  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout1().host_size, view()->size());
  EnsureLayout(layout1());
}

TEST_F(AnimatingLayoutManagerNoAnimationsTest, ChangeLayoutNoAnimation) {
  UseFixedLayout(layout1());
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);

  SizeAndLayout();
  UseFixedLayout(layout2());
  view()->InvalidateLayout();
  EXPECT_FALSE(layout()->is_animating());
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(layout2().host_size, view()->size());
  EnsureLayout(layout2());
}

TEST_F(AnimatingLayoutManagerNoAnimationsTest, HideShowViewNoAnimation) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  UseFlexLayout()
      ->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetDefault(kMarginsKey, gfx::Insets(5));
  const ProposedLayout expected_layout{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_layout);

  for (const auto& fade_mode : kFadeModes) {
    layout()->SetDefaultFadeMode(fade_mode.first);

    child(0)->SetVisible(false);
    EXPECT_FALSE(layout()->is_animating());
    view()->InvalidateLayout();
    EXPECT_FALSE(layout()->is_animating());
    SizeAndLayout();
    EXPECT_FALSE(layout()->is_animating());
    const ProposedLayout expected_layout2 = {
        {35, 20},
        {{child(0), false},
         {child(1), true, {{5, 5}, kChildViewSize}},
         {child(2), true, {{20, 5}, kChildViewSize}}}};
    EnsureLayout(expected_layout2, fade_mode.second);

    child(0)->SetVisible(true);
    EXPECT_FALSE(layout()->is_animating());
    view()->InvalidateLayout();
    EXPECT_FALSE(layout()->is_animating());
    SizeAndLayout();
    EXPECT_FALSE(layout()->is_animating());
    EnsureLayout(expected_layout, fade_mode.second);
  }
}

TEST_F(AnimatingLayoutManagerNoAnimationsTest, FadeViewInOutNoAnimation) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  UseFlexLayout()
      ->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCollapseMargins(true)
      .SetDefault(kMarginsKey, gfx::Insets(5));
  const ProposedLayout expected_layout{
      {50, 20},
      {{child(0), true, {{5, 5}, kChildViewSize}},
       {child(1), true, {{20, 5}, kChildViewSize}},
       {child(2), true, {{35, 5}, kChildViewSize}}}};
  SizeAndLayout();
  EXPECT_FALSE(layout()->is_animating());
  EnsureLayout(expected_layout);

  for (const auto& fade_mode : kFadeModes) {
    layout()->SetDefaultFadeMode(fade_mode.first);

    layout()->FadeOut(child(0));
    EXPECT_FALSE(layout()->is_animating());
    view()->InvalidateLayout();
    EXPECT_FALSE(layout()->is_animating());
    SizeAndLayout();
    EXPECT_FALSE(layout()->is_animating());
    const ProposedLayout expected_layout2 = {
        {35, 20},
        {{child(0), false},
         {child(1), true, {{5, 5}, kChildViewSize}},
         {child(2), true, {{20, 5}, kChildViewSize}}}};
    EnsureLayout(expected_layout2, fade_mode.second);

    layout()->FadeIn(child(0));
    EXPECT_FALSE(layout()->is_animating());
    view()->InvalidateLayout();
    EXPECT_FALSE(layout()->is_animating());
    SizeAndLayout();
    EXPECT_FALSE(layout()->is_animating());
    EnsureLayout(expected_layout, fade_mode.second);
  }
}

TEST_F(AnimatingLayoutManagerNoAnimationsTest, ActionsPostedAfterLayout) {
  UseFixedLayout(layout1());
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  SizeAndLayout();

  bool cb1 = false;
  bool cb2 = false;
  bool cb3 = false;
  bool cb4 = false;

  // We're in a non-animating, not-waiting-for layout state, so an action
  // should post immediately.
  layout()->PostOrQueueAction(
      base::BindLambdaForTesting([&]() { cb1 = true; }));
  RunCurrentTasks();
  EXPECT_TRUE(cb1);

  // Changing the layout puts us in a state where we're waiting for the layout
  // to be performed, so actions will not post yet.
  UseFixedLayout(layout2());
  layout()->PostOrQueueAction(
      base::BindLambdaForTesting([&]() { cb2 = true; }));
  view()->InvalidateLayout();
  layout()->PostOrQueueAction(
      base::BindLambdaForTesting([&]() { cb3 = true; }));
  RunCurrentTasks();
  EXPECT_FALSE(cb2);
  EXPECT_FALSE(cb3);

  // Layout will post the pending actions.
  SizeAndLayout();
  RunCurrentTasks();
  EXPECT_TRUE(cb2);
  EXPECT_TRUE(cb3);

  // Now that we've laid out and there are no additional changes, we are free
  // to post immediately again.
  layout()->PostOrQueueAction(
      base::BindLambdaForTesting([&]() { cb4 = true; }));
  RunCurrentTasks();
  EXPECT_TRUE(cb4);
}

namespace {

constexpr base::TimeDelta kMinimumAnimationTime = base::Milliseconds(50);

// Layout manager which immediately lays out its child views when it is
// invalidated.
class ImmediateLayoutManager : public LayoutManagerBase {
 public:
  ImmediateLayoutManager()
      : ImmediateLayoutManager(
            AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes,
            LayoutOrientation::kHorizontal) {}

  ImmediateLayoutManager(
      AnimatingLayoutManager::BoundsAnimationMode bounds_animation_mode,
      LayoutOrientation orientation,
      SizeBounds size_bounds = SizeBounds())
      : bounds_animation_mode_(bounds_animation_mode),
        orientation_(orientation),
        size_bounds_(std::move(size_bounds)) {}

  ~ImmediateLayoutManager() override = default;

  // LayoutManager:

  void OnLayoutChanged() override {
    LayoutManagerBase::OnLayoutChanged();
    // Host needs to be invalidated in order for a layout to be scheduled. Pass
    // in false for mark_layouts_changed since we don't want OnLayoutChanged()
    // to be called again.
    InvalidateHost(false);
    test::RunScheduledLayout(host_view());
  }

  ProposedLayout CalculateProposedLayout(
      const SizeBounds& bounds) const override {
    ProposedLayout layout;
    for (View* child : host_view()->children()) {
      if (!IsChildIncludedInLayout(child))
        continue;
      ChildLayout child_layout;
      child_layout.child_view = child;
      child_layout.visible = child->GetVisible();
      child_layout.available_size = size_bounds_;
      switch (bounds_animation_mode_) {
        case AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes:
          child_layout.bounds = gfx::Rect(ConstrainSizeToBounds(
              child->GetPreferredSize(bounds), size_bounds_));
          break;
        case AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis: {
          // Start with the preferred size constrained to the bounds, then force
          // the cross axis.
          gfx::Size size = ConstrainSizeToBounds(
              child->GetPreferredSize(bounds), size_bounds_);
          SetCrossAxis(&size, orientation_,
                       GetCrossAxis(orientation_, child->bounds().size()));
          child_layout.bounds = gfx::Rect(size);
        } break;
        case AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds:
          child_layout.bounds = child->bounds();
          break;
      }
      layout.host_size.SetToMax(child_layout.bounds.size());
      layout.child_layouts.push_back(child_layout);
    }
    return layout;
  }

  void SetSizeBounds(const SizeBounds& size_bounds) {
    size_bounds_ = size_bounds;
    OnLayoutChanged();
    GetProposedLayout(host_view()->size());
  }

 private:
  const AnimatingLayoutManager::BoundsAnimationMode bounds_animation_mode_;
  const LayoutOrientation orientation_;
  SizeBounds size_bounds_;
};

// Allows an AnimatingLayoutManager to be observed so that we can wait for an
// animation to complete in real time. Call WaitForAnimationToComplete() to
// pause execution until an animation (if any) is completed.
class AnimationWatcher : public AnimatingLayoutManager::Observer {
 public:
  explicit AnimationWatcher(AnimatingLayoutManager* layout_manager)
      : layout_manager_(layout_manager) {
    observation_.Observe(layout_manager);
  }
  ~AnimationWatcher() override = default;

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
  const raw_ptr<AnimatingLayoutManager> layout_manager_;
  base::ScopedObservation<AnimatingLayoutManager,
                          AnimatingLayoutManager::Observer>
      observation_{this};
  std::unique_ptr<base::RunLoop> run_loop_;
  bool waiting_ = false;
};

}  // anonymous namespace

// Test which explores an animating layout manager's response to available size
// changes.
class AnimatingLayoutManagerRootViewTest : public AnimatingLayoutManagerTest {
 public:
  explicit AnimatingLayoutManagerRootViewTest(bool enable_animations = true)
      : AnimatingLayoutManagerTest(enable_animations) {}
  ~AnimatingLayoutManagerRootViewTest() override = default;

  void SetUp() override {
    AnimatingLayoutManagerTest::SetUp();
    root_view_ = std::make_unique<View>();
    root_view_->AddChildView(view());
  }

  void TearDown() override {
    // Don't call base version because we own the view.
  }

  View* root_view() { return root_view_.get(); }

 private:
  std::unique_ptr<View> root_view_;
};

// Available Size Tests --------------------------------------------------------

class AnimatingLayoutManagerAvailableSizeTest
    : public AnimatingLayoutManagerRootViewTest {
 protected:
  void InitRootView() {
    root_layout_ =
        root_view()->SetLayoutManager(std::make_unique<ImmediateLayoutManager>(
            layout()->bounds_animation_mode(), layout()->orientation()));
  }

  ImmediateLayoutManager* root_layout() { return root_layout_; }

 private:
  raw_ptr<ImmediateLayoutManager> root_layout_;
};

TEST_F(AnimatingLayoutManagerAvailableSizeTest, AvailableSize_LimitsExpansion) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  InitRootView();

  child(1)->SetVisible(false);
  child(2)->SetVisible(false);
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Verify the initial layout.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->GetPreferredSize({}));
  EXPECT_EQ(gfx::Size(20, 20), view()->size());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());

  // Set the root view bounds.
  root_layout()->SetSizeBounds({40, 25});
  child(1)->SetVisible(true);
  child(2)->SetVisible(true);

  // Animation should have started.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->GetPreferredSize({}));

  // Complete the animation.
  AnimationEventLogger logger(layout());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(35, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 5, 10, 10), child(1)->bounds());
  EXPECT_FALSE(child(2)->GetVisible());

  const std::vector<bool> expected_events{false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_RestartsAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  InitRootView();

  child(1)->SetVisible(false);
  child(2)->SetVisible(false);
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Set the root view bounds and trigger an animation.
  root_layout()->SetSizeBounds({40, 25});
  child(1)->SetVisible(true);
  child(2)->SetVisible(true);
  EXPECT_TRUE(layout()->is_animating());

  // Complete the animation.
  AnimationEventLogger logger(layout());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());

  // Unconstrain the bounds and do another layout.
  root_layout()->SetSizeBounds({80, 30});
  test::RunScheduledLayout(root_view());
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(50, 20), view()->GetPreferredSize({}));
  EXPECT_EQ(gfx::Size(50, 20), view()->size());

  const std::vector<bool> expected_events{false, true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_RestartsAnimation_Vertical) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kVertical);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  InitRootView();

  child(1)->SetVisible(false);
  child(2)->SetVisible(false);
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Set the root view bounds and trigger an animation.
  root_layout()->SetSizeBounds({25, 40});
  child(1)->SetVisible(true);
  child(2)->SetVisible(true);
  EXPECT_TRUE(layout()->is_animating());

  // Complete the animation.
  AnimationEventLogger logger(layout());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 20, 10, 10), child(1)->bounds());
  EXPECT_FALSE(child(2)->GetVisible());

  // Unconstrain the bounds and do another layout.
  root_layout()->SetSizeBounds({30, 80});
  test::RunScheduledLayout(root_view());
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 50), view()->GetPreferredSize({}));
  EXPECT_EQ(gfx::Size(20, 50), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 20, 10, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 35, 10, 10), child(2)->bounds());

  const std::vector<bool> expected_events{false, true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_RedirectsAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  InitRootView();

  child(1)->SetVisible(false);
  child(2)->SetVisible(false);
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Kick off an animation to full size.
  child(1)->SetVisible(true);
  child(2)->SetVisible(true);
  EXPECT_TRUE(layout()->is_animating());

  // Set the root view bounds larger than the expected size halfway through the
  // animation (35 px wide), but smaller than the target (50px wide).
  AnimationEventLogger logger(layout());
  root_layout()->SetSizeBounds({45, 25});
  animation_api()->IncrementTime(base::Milliseconds(500));

  // This should redirect the animation.
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(35, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 5, 10, 10), child(1)->bounds());
  EXPECT_FALSE(child(2)->GetVisible());

  const std::vector<bool> expected_events{false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest, AvailableSize_StopsAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  InitRootView();

  child(1)->SetVisible(false);
  child(2)->SetVisible(false);
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Kick off an animation to full size.
  child(1)->SetVisible(true);
  child(2)->SetVisible(true);
  EXPECT_TRUE(layout()->is_animating());

  // Set the root view bounds smaller than the expected size halfway through the
  // animation (35 px wide).
  AnimationEventLogger logger(layout());
  animation_api()->IncrementTime(base::Milliseconds(500));
  root_layout()->SetSizeBounds({25, 25});

  // This should stop the animation.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  const std::vector<bool> expected_events{false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest, AvailableSize_ImmediateResize) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  InitRootView();
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Set the root view bounds smaller than the expected size.
  AnimationEventLogger logger(layout());
  root_layout()->SetSizeBounds({25, 25});
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  const std::vector<bool> expected_events{};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest, AvailableSize_StepDownStepUp) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  InitRootView();
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Set the root view bounds smaller than the expected size.
  AnimationEventLogger logger(layout());
  root_layout()->SetSizeBounds({25, 25});
  test::RunScheduledLayout(root_view());

  // Step down again to a tighter bound. Should not result in animation.
  root_layout()->SetSizeBounds({20, 20});
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  // Step back up. Should not result in animation.
  root_layout()->SetSizeBounds({30, 30});
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  const std::vector<bool> expected_events{};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_ConstraintRemovedStartsAnimation) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  InitRootView();
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Set the root view bounds smaller than the expected size.
  AnimationEventLogger logger(layout());
  root_layout()->SetSizeBounds({25, 25});
  test::RunScheduledLayout(root_view());

  // Remove the constraint. This should start an animation.
  root_layout()->SetSizeBounds(SizeBounds());
  test::RunScheduledLayout(root_view());
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(50, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 5, 10, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(35, 5, 10, 10), child(2)->bounds());

  const std::vector<bool> expected_events{true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_LimitsExpansion_WithFlex) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  child(1)->SetProperty(kFlexBehaviorKey, kFlex);
  InitRootView();

  child(1)->SetVisible(false);
  child(2)->SetVisible(false);
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Verify the initial layout.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->GetPreferredSize({}));
  EXPECT_EQ(gfx::Size(20, 20), view()->size());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());

  // Set the root view bounds.
  root_layout()->SetSizeBounds({45, 25});
  child(1)->SetVisible(true);
  child(2)->SetVisible(true);

  // Animation should have started.
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->GetPreferredSize({}));

  // Complete the animation.
  AnimationEventLogger logger(layout());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(45, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 5, 5, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(30, 5, 10, 10), child(2)->bounds());

  const std::vector<bool> expected_events{false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_RestartsAnimation_WithFlex) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  child(1)->SetProperty(kFlexBehaviorKey, kFlex);
  InitRootView();

  child(1)->SetVisible(false);
  child(2)->SetVisible(false);
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Set the root view bounds and trigger an animation.
  root_layout()->SetSizeBounds({45, 25});
  child(1)->SetVisible(true);
  child(2)->SetVisible(true);
  EXPECT_TRUE(layout()->is_animating());

  // Complete the animation.
  AnimationEventLogger logger(layout());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());

  // Unconstrain the bounds and do another layout.
  root_layout()->SetSizeBounds({80, 30});
  test::RunScheduledLayout(root_view());
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(50, 20), view()->GetPreferredSize({}));
  EXPECT_EQ(gfx::Size(50, 20), view()->size());

  const std::vector<bool> expected_events{false, true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_RedirectsAnimation_WithFlex) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  child(1)->SetProperty(kFlexBehaviorKey, kFlex);
  InitRootView();

  child(1)->SetVisible(false);
  child(2)->SetVisible(false);
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Kick off an animation to full size.
  child(1)->SetVisible(true);
  child(2)->SetVisible(true);
  EXPECT_TRUE(layout()->is_animating());

  // Set the root view bounds larger than the expected size halfway through the
  // animation (35 px wide), but smaller than the target (50px wide).
  AnimationEventLogger logger(layout());
  root_layout()->SetSizeBounds({45, 25});
  animation_api()->IncrementTime(base::Milliseconds(500));

  // This should redirect the animation.
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(45, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 5, 5, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(30, 5, 10, 10), child(2)->bounds());

  const std::vector<bool> expected_events{false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_StopsAnimation_WithFlex) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  child(1)->SetProperty(kFlexBehaviorKey, kFlex);
  InitRootView();

  child(1)->SetVisible(false);
  child(2)->SetVisible(false);
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Kick off an animation to full size.
  child(1)->SetVisible(true);
  child(2)->SetVisible(true);
  EXPECT_TRUE(layout()->is_animating());

  // Set the root view bounds smaller than the expected size halfway through the
  // animation (35 px wide).
  AnimationEventLogger logger(layout());
  animation_api()->IncrementTime(base::Milliseconds(500));
  root_layout()->SetSizeBounds({25, 25});

  // This should stop the animation.
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  const std::vector<bool> expected_events{false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_ImmediateResize_WithFlex) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  child(1)->SetProperty(kFlexBehaviorKey, kFlex);
  InitRootView();
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Set the root view bounds smaller than the expected size.
  AnimationEventLogger logger(layout());
  root_layout()->SetSizeBounds({25, 25});
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  const std::vector<bool> expected_events{};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_StepDownStepUp_WithFlex) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  InitRootView();
  layout()->ResetLayout();
  view()->InvalidateLayout();

  // Set the root view bounds smaller than the expected size.
  AnimationEventLogger logger(layout());
  root_layout()->SetSizeBounds({25, 25});
  test::RunScheduledLayout(root_view());

  // Step down again to a tighter bound. Should not result in animation.
  root_layout()->SetSizeBounds({20, 20});
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  // Step back up. Should not result in animation.
  root_layout()->SetSizeBounds({30, 30});
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  const std::vector<bool> expected_events{};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AvailableSize_ConstraintRemovedStartsAnimation_WithFlex) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  flex_layout->SetDefault(kFlexBehaviorKey, kDropOut);
  child(1)->SetProperty(kFlexBehaviorKey, kFlex);
  InitRootView();
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  // Set the root view bounds smaller than the expected size.
  AnimationEventLogger logger(layout());
  root_layout()->SetSizeBounds({25, 25});
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());

  // Remove the constraint. This should start an animation.
  root_layout()->SetSizeBounds({60, 25});
  test::RunScheduledLayout(root_view());
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(50, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 5, 10, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(35, 5, 10, 10), child(2)->bounds());

  const std::vector<bool> expected_events{true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AnimateMainAxis_Horizontal_MainAxisAnimates) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(LayoutAlignment::kStart)
      .SetDefault(kFlexBehaviorKey,
                  FlexSpecification(LayoutOrientation::kHorizontal,
                                    MinimumFlexSizeRule::kPreferred,
                                    MaximumFlexSizeRule::kPreferred, false,
                                    MinimumFlexSizeRule::kScaleToZero));
  view()->SetBoundsRect(gfx::Rect(0, 0, 5, 5));
  InitRootView();
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(30, 5), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 5), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 5), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 0, 10, 5), child(2)->bounds());

  child(1)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());

  // Advance the animation halfway.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(25, 5), view()->size());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 5), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 5), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 5), child(2)->bounds());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AnimateMainAxis_Vertical_MainAxisAnimates) {
  layout()
      ->SetBoundsAnimationMode(
          AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis)
      .SetOrientation(LayoutOrientation::kVertical);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(LayoutAlignment::kStart)
      .SetDefault(kFlexBehaviorKey,
                  FlexSpecification(LayoutOrientation::kVertical,
                                    MinimumFlexSizeRule::kPreferred,
                                    MaximumFlexSizeRule::kPreferred, false,
                                    MinimumFlexSizeRule::kScaleToZero));
  view()->SetBoundsRect(gfx::Rect(0, 0, 5, 5));
  InitRootView();
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(5, 30), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 10, 5, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 20, 5, 10), child(2)->bounds());

  child(1)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());

  // Advance the animation halfway.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(5, 25), view()->size());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(5, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 10), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 10, 5, 10), child(2)->bounds());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AnimateMainAxis_Horizontal_CrossAxisSizeChangeResetsLayout) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(LayoutAlignment::kStart)
      .SetDefault(kFlexBehaviorKey,
                  FlexSpecification(LayoutOrientation::kHorizontal,
                                    MinimumFlexSizeRule::kPreferred,
                                    MaximumFlexSizeRule::kPreferred, false,
                                    MinimumFlexSizeRule::kScaleToZero));
  view()->SetBoundsRect(gfx::Rect(0, 0, 5, 5));
  InitRootView();
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(30, 5), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 5), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 5), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 0, 10, 5), child(2)->bounds());

  child(1)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());

  // Advance the animation halfway.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(25, 5), view()->size());

  // Change the cross-axis size.
  view()->SetSize(gfx::Size(25, 7));
  view()->InvalidateLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 7), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 7), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 7), child(2)->bounds());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AnimateMainAxis_Vertical_CrossAxisSizeChangeResetsLayout) {
  layout()
      ->SetBoundsAnimationMode(
          AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis)
      .SetOrientation(LayoutOrientation::kVertical);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(LayoutAlignment::kStart)
      .SetDefault(kFlexBehaviorKey,
                  FlexSpecification(LayoutOrientation::kVertical,
                                    MinimumFlexSizeRule::kPreferred,
                                    MaximumFlexSizeRule::kPreferred, false,
                                    MinimumFlexSizeRule::kScaleToZero));
  view()->SetBoundsRect(gfx::Rect(0, 0, 5, 5));
  InitRootView();
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(5, 30), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 10, 5, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 20, 5, 10), child(2)->bounds());

  child(1)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());

  // Advance the animation halfway.
  animation_api()->IncrementTime(base::Milliseconds(500));
  EXPECT_TRUE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(5, 25), view()->size());

  // Change the cross-axis size.
  view()->SetSize(gfx::Size(7, 25));
  view()->InvalidateLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(7, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 7, 10), child(0)->bounds());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 10, 7, 10), child(2)->bounds());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AnimateMainAxis_Horizontal_CrossAxisAlignmentWorks) {
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(LayoutAlignment::kStart);
  // Pick an arbitrary (wrong) main-axis size.
  view()->SetBoundsRect(gfx::Rect(0, 0, 20, 20));
  InitRootView();
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(30, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 0, 10, 10), child(2)->bounds());

  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(30, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 5, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 5, 10, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 5, 10, 10), child(2)->bounds());

  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(30, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 10, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 10, 10, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 10, 10, 10), child(2)->bounds());

  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(30, 20), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 20), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(20, 0, 10, 20), child(2)->bounds());
}

TEST_F(AnimatingLayoutManagerAvailableSizeTest,
       AnimateMainAxis_Vertical_CrossAxisAlignmentWorks) {
  layout()
      ->SetBoundsAnimationMode(
          AnimatingLayoutManager::BoundsAnimationMode::kAnimateMainAxis)
      .SetOrientation(LayoutOrientation::kVertical);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(LayoutAlignment::kStart);
  // Pick an arbitrary (wrong) main-axis size.
  view()->SetBoundsRect(gfx::Rect(0, 0, 20, 20));
  InitRootView();
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 30), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 10, 10, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 20, 10, 10), child(2)->bounds());

  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kCenter);
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 30), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 0, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 10, 10, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(5, 20, 10, 10), child(2)->bounds());

  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kEnd);
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 30), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 10, 10, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(10, 20, 10, 10), child(2)->bounds());

  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStretch);
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(1000));
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 30), view()->size());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), child(0)->bounds());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 10), child(1)->bounds());
  EXPECT_TRUE(child(2)->GetVisible());
  EXPECT_EQ(gfx::Rect(0, 20, 20, 10), child(2)->bounds());
}

// Flex Rule Tests -------------------------------------------------------------

class AnimatingLayoutManagerFlexRuleTest : public AnimatingLayoutManagerTest {
 public:
  AnimatingLayoutManagerFlexRuleTest() = default;
  ~AnimatingLayoutManagerFlexRuleTest() override = default;

  void InitLayout(LayoutOrientation orientation,
                  const FlexSpecification& default_flex,
                  const std::optional<gfx::Size>& minimum_size,
                  bool fix_child_size) {
    for (size_t i = 0; i < num_children(); ++i) {
      if (minimum_size)
        child(i)->SetMinimumSize(*minimum_size);
      if (fix_child_size)
        child(i)->SetFixArea(true);
    }
    layout()->SetOrientation(orientation);
    layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
    flex_layout()->SetOrientation(orientation);
    flex_layout()->SetCollapseMargins(true);
    flex_layout()->SetDefault(kMarginsKey, gfx::Insets(5));
    flex_layout()->SetDefault(kFlexBehaviorKey, default_flex);
    flex_rule_ = layout()->GetDefaultFlexRule();
  }

  size_t GetVisibleChildCount(const gfx::Size& size) {
    ProposedLayout layout = flex_layout()->GetProposedLayout(size);
    EXPECT_EQ(size, layout.host_size);
    return base::ranges::count_if(layout.child_layouts, &ChildLayout::visible);
  }

  FlexLayout* flex_layout() {
    return static_cast<FlexLayout*>(layout()->target_layout_manager());
  }

  gfx::Size RunFlexRule(const SizeBounds& bounds) const {
    return flex_rule_.Run(view(), bounds);
  }

  static const FlexSpecification kScaleToMinimumSnapToZero;

 private:
  FlexRule flex_rule_;
};

const FlexSpecification
    AnimatingLayoutManagerFlexRuleTest::kScaleToMinimumSnapToZero =
        FlexSpecification(MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                          MaximumFlexSizeRule::kUnbounded,
                          true)
            .WithOrder(2);

TEST_F(AnimatingLayoutManagerFlexRuleTest, ReturnsPreferredSize) {
  InitLayout(LayoutOrientation::kHorizontal, kScaleToMinimumSnapToZero,
             gfx::Size(5, 5), false);
  EXPECT_EQ(flex_layout()->GetPreferredSize(view()), RunFlexRule(SizeBounds()));
}

TEST_F(AnimatingLayoutManagerFlexRuleTest,
       VerticalBounded_ReturnsPreferredSize) {
  InitLayout(LayoutOrientation::kVertical, kScaleToMinimumSnapToZero,
             gfx::Size(5, 5), true);
  const gfx::Size preferred = flex_layout()->GetPreferredSize(view());
  const gfx::Size result =
      RunFlexRule(SizeBounds(preferred.width() + 5, SizeBound()));
  EXPECT_EQ(preferred, result);
  EXPECT_EQ(3U, GetVisibleChildCount(result));
}

TEST_F(AnimatingLayoutManagerFlexRuleTest,
       VerticalBounded_ReturnsHeightForWidth) {
  InitLayout(LayoutOrientation::kVertical, kScaleToMinimumSnapToZero,
             gfx::Size(5, 5), true);

  const gfx::Size preferred = flex_layout()->GetPreferredSize(view());
  const int width = preferred.width() - 5;
  const int height_for_width =
      flex_layout()->GetPreferredHeightForWidth(view(), width);
  DCHECK_GT(height_for_width, preferred.height());
  const gfx::Size result = RunFlexRule(SizeBounds(width, SizeBound()));
  EXPECT_EQ(gfx::Size(width, height_for_width), result);
  EXPECT_EQ(3U, GetVisibleChildCount(result));
}

TEST_F(AnimatingLayoutManagerFlexRuleTest, HorizontalBounded_FlexToSize) {
  InitLayout(LayoutOrientation::kHorizontal, kScaleToMinimumSnapToZero,
             gfx::Size(5, 5), false);

  const gfx::Size preferred = flex_layout()->GetPreferredSize(view());
  const gfx::Size actual(preferred.width() - 5, preferred.height());
  const ProposedLayout layout = flex_layout()->GetProposedLayout(actual);
  DCHECK_LT(layout.host_size.width(), preferred.width());
  const gfx::Size result = RunFlexRule(SizeBounds(actual));
  EXPECT_EQ(actual, result);
  EXPECT_EQ(3U, GetVisibleChildCount(result));
}

TEST_F(AnimatingLayoutManagerFlexRuleTest, HorizontalBounded_DropOut) {
  InitLayout(LayoutOrientation::kHorizontal, kDropOut, {}, false);

  const gfx::Size preferred = flex_layout()->GetPreferredSize(view());
  const gfx::Size actual(preferred.width() - 5, preferred.height());
  const ProposedLayout layout = flex_layout()->GetProposedLayout(actual);
  DCHECK_LT(layout.host_size.width(), actual.width());
  const gfx::Size result = RunFlexRule(SizeBounds(actual));
  EXPECT_EQ(layout.host_size, result);
  EXPECT_EQ(2U, GetVisibleChildCount(result));
}

TEST_F(AnimatingLayoutManagerFlexRuleTest, VerticalBounded_FlexToSize) {
  InitLayout(LayoutOrientation::kVertical, kScaleToMinimumSnapToZero,
             gfx::Size(5, 5), false);

  const gfx::Size preferred = flex_layout()->GetPreferredSize(view());
  const gfx::Size actual(preferred.width(), preferred.height() - 5);
  const ProposedLayout layout = flex_layout()->GetProposedLayout(actual);
  DCHECK_LT(layout.host_size.height(), preferred.height());
  const gfx::Size result = RunFlexRule(SizeBounds(actual));
  EXPECT_EQ(actual, result);
  EXPECT_EQ(3U, GetVisibleChildCount(result));
}

TEST_F(AnimatingLayoutManagerFlexRuleTest, VerticalBounded_DropOut) {
  InitLayout(LayoutOrientation::kVertical, kDropOut, {}, false);

  const gfx::Size preferred = flex_layout()->GetPreferredSize(view());
  const gfx::Size actual(preferred.width(), preferred.height() - 5);
  const ProposedLayout layout = flex_layout()->GetProposedLayout(actual);
  DCHECK_LT(layout.host_size.height(), actual.height());
  const gfx::Size result = RunFlexRule(SizeBounds(actual));
  EXPECT_EQ(layout.host_size, result);
  EXPECT_EQ(2U, GetVisibleChildCount(result));
}

TEST_F(AnimatingLayoutManagerFlexRuleTest, HorizontalDoubleBounded_DropOut) {
  InitLayout(LayoutOrientation::kHorizontal, kScaleToMinimumSnapToZero,
             gfx::Size(10, 5), true);

  const gfx::Size preferred = flex_layout()->GetPreferredSize(view());
  const gfx::Size actual(preferred.width() - 5, preferred.height() - 5);
  const ProposedLayout layout = flex_layout()->GetProposedLayout(actual);
  DCHECK_LT(layout.host_size.width(), preferred.width());
  DCHECK_LT(layout.host_size.height(), preferred.height());
  const gfx::Size result = RunFlexRule(SizeBounds(actual));
  EXPECT_EQ(layout.host_size, result);
  EXPECT_EQ(2U, GetVisibleChildCount(result));
}

TEST_F(AnimatingLayoutManagerFlexRuleTest, VerticalDoubleBounded_DropOut) {
  InitLayout(LayoutOrientation::kVertical, kScaleToMinimumSnapToZero,
             gfx::Size(5, 10), true);

  const gfx::Size preferred = flex_layout()->GetPreferredSize(view());
  const gfx::Size actual(preferred.width() - 5, preferred.height() - 5);
  const ProposedLayout layout = flex_layout()->GetProposedLayout(actual);
  DCHECK_LT(layout.host_size.width(), preferred.width());
  DCHECK_LT(layout.host_size.height(), preferred.height());
  const gfx::Size result = RunFlexRule(SizeBounds(actual));
  EXPECT_EQ(layout.host_size, result);
  EXPECT_EQ(2U, GetVisibleChildCount(result));
}

TEST_F(AnimatingLayoutManagerFlexRuleTest, HorizontalMinimumSize) {
  InitLayout(LayoutOrientation::kHorizontal, kScaleToMinimumSnapToZero,
             gfx::Size(5, 5), true);

  const gfx::Size minimum = flex_layout()->GetMinimumSize(view());
  const gfx::Size preferred = flex_layout()->GetPreferredSize(view());
  DCHECK_GT(preferred.width(), minimum.width());
  DCHECK_GT(preferred.height(), minimum.height());
  const gfx::Size result = RunFlexRule(SizeBounds(0, 0));
  EXPECT_EQ(minimum, result);
}

TEST_F(AnimatingLayoutManagerFlexRuleTest, VerticalMinimumSize) {
  InitLayout(LayoutOrientation::kVertical, kScaleToMinimumSnapToZero,
             gfx::Size(5, 5), true);

  const gfx::Size minimum = flex_layout()->GetMinimumSize(view());
  const gfx::Size preferred = flex_layout()->GetPreferredSize(view());
  DCHECK_GT(preferred.width(), minimum.width());
  DCHECK_GT(preferred.height(), minimum.height());
  const gfx::Size result = RunFlexRule(SizeBounds(0, 0));
  EXPECT_EQ(minimum, result);
}

// Animating Layout in Flex Layout ---------------------------------------------

class AnimatingLayoutManagerInFlexLayoutTest
    : public AnimatingLayoutManagerRootViewTest {
 protected:
  explicit AnimatingLayoutManagerInFlexLayoutTest(bool enable_animations = true)
      : AnimatingLayoutManagerRootViewTest(enable_animations) {}
  ~AnimatingLayoutManagerInFlexLayoutTest() override = default;

  void SetUp() override {
    AnimatingLayoutManagerRootViewTest::SetUp();
    layout()->SetBoundsAnimationMode(
        AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
    root_layout_ =
        root_view()->SetLayoutManager(std::make_unique<FlexLayout>());
    root_layout_->SetOrientation(LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(LayoutAlignment::kStart)
        .SetCrossAxisAlignment(LayoutAlignment::kStart);
    view()->SetProperty(
        kFlexBehaviorKey,
        FlexSpecification(layout()->GetDefaultFlexRule()).WithOrder(2));
    target_layout_ =
        layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
    target_layout_->SetOrientation(LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(LayoutAlignment::kStart)
        .SetCrossAxisAlignment(LayoutAlignment::kStart)
        .SetCollapseMargins(true)
        .SetDefault(kMarginsKey, gfx::Insets(5))
        .SetDefault(kFlexBehaviorKey, kDropOut);
    other_view_ = root_view()->AddChildView(std::make_unique<TestView>());
  }

  FlexLayout* root_layout() { return root_layout_; }
  FlexLayout* target_layout() { return target_layout_; }
  TestView* other_view() { return other_view_; }

 private:
  raw_ptr<FlexLayout> root_layout_;
  raw_ptr<FlexLayout> target_layout_;
  raw_ptr<TestView, DanglingUntriaged> other_view_;
};

TEST_F(AnimatingLayoutManagerInFlexLayoutTest, NoAnimation) {
  other_view()->set_preferred_size(gfx::Size());
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  AnimationEventLogger logger(layout());
  test::RunScheduledLayout(root_view());
  EXPECT_EQ(preferred, view()->size());
  const std::vector<bool> expected_events{};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest,
       AnimateFullyWithinAvailableSpace) {
  other_view()->set_preferred_size(gfx::Size());
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());
  AnimationEventLogger logger(layout());

  // Shrink the view by hiding a child view.
  child(0)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(target_layout()->GetPreferredSize(view()), view()->size());
  EXPECT_EQ(child(1)->size(), kChildViewSize);
  EXPECT_EQ(child(2)->size(), kChildViewSize);

  // Grow the view back to its original size.
  child(0)->SetVisible(true);
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(target_layout()->GetPreferredSize(view()), view()->size());
  EXPECT_EQ(child(0)->size(), kChildViewSize);
  EXPECT_EQ(child(1)->size(), kChildViewSize);
  EXPECT_EQ(child(2)->size(), kChildViewSize);

  // Verify the event log.
  const std::vector<bool> expected_events{true, false, true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest, NoAnimationRestart) {
  other_view()->set_preferred_size(gfx::Size());
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());
  AnimationEventLogger logger(layout());

  // Shrink the view by hiding a child view.
  child(0)->SetVisible(false);
  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(root_view());
  // Do an extra layout.
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(target_layout()->GetPreferredSize(view()), view()->size());

  // Grow the view back to its original size.
  child(0)->SetVisible(true);
  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(root_view());
  // Do an extra layout.
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(target_layout()->GetPreferredSize(view()), view()->size());

  // Verify the event log.
  const std::vector<bool> expected_events{true, false, true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest, GrowWithinConstrainedSpace) {
  other_view()->set_preferred_size(gfx::Size(5, 5));
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());
  AnimationEventLogger logger(layout());

  // Shrink the view by hiding child views.
  child(0)->SetVisible(false);
  child(1)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->size());
  EXPECT_EQ(child(2)->size(), kChildViewSize);

  // Grow the view back to a constrained size.
  child(0)->SetVisible(true);
  child(1)->SetVisible(true);
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(35, 20), view()->size());
  EXPECT_EQ(child(0)->size(), kChildViewSize);
  EXPECT_EQ(child(1)->size(), kChildViewSize);
  EXPECT_FALSE(child(2)->GetVisible());

  // Verify the event log.
  const std::vector<bool> expected_events{true, false, true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest,
       GrowWithinConstrainedSpace_NoAnimationRestart) {
  other_view()->set_preferred_size(gfx::Size(5, 5));
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());
  AnimationEventLogger logger(layout());

  // Shrink the view by hiding child views.
  child(0)->SetVisible(false);
  child(1)->SetVisible(false);
  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(root_view());
  // Do an extra layout.
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->size());

  // Grow the view back to a constrained size.
  child(0)->SetVisible(true);
  child(1)->SetVisible(true);
  animation_api()->IncrementTime(base::Milliseconds(1000));
  test::RunScheduledLayout(root_view());
  // Do an extra layout.
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(35, 20), view()->size());

  // Verify the event log.
  const std::vector<bool> expected_events{true, false, true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest,
       GrowWithinConstrainedSpace_AnimationRedirected) {
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  // Shrink the view by hiding child views.
  child(0)->SetVisible(false);
  child(1)->SetVisible(false);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->size());

  AnimationEventLogger logger(layout());

  // Grow the view back to full size.
  child(0)->SetVisible(true);
  child(1)->SetVisible(true);
  animation_api()->IncrementTime(base::Milliseconds(500));

  // Constrain the layout before continuing.
  other_view()->set_preferred_size(gfx::Size(5, 5));
  test::RunScheduledLayout(root_view());
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(35, 20), view()->size());
  EXPECT_EQ(child(0)->size(), kChildViewSize);
  EXPECT_EQ(child(1)->size(), kChildViewSize);
  EXPECT_FALSE(child(2)->GetVisible());

  // Verify the event log.
  const std::vector<bool> expected_events{true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest,
       GrowWithinConstrainedSpace_AnimationInterrupted) {
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  // Shrink the view by hiding child views.
  child(0)->SetVisible(false);
  child(1)->SetVisible(false);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->size());

  AnimationEventLogger logger(layout());

  // Grow the view back to full size.
  child(0)->SetVisible(true);
  child(1)->SetVisible(true);
  animation_api()->IncrementTime(base::Milliseconds(500));

  // Constrain the layout before continuing, killing the animation.
  other_view()->set_preferred_size(gfx::Size(20, 5));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->size());
  EXPECT_EQ(child(0)->size(), kChildViewSize);
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  // Verify the event log.
  const std::vector<bool> expected_events{true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest,
       ShrinkWithinConstrainedSpace_AnimationProceeds) {
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  AnimationEventLogger logger(layout());

  // Shrink the view by hiding child views.
  child(0)->SetVisible(false);
  child(1)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(500));

  // Constrain the layout before continuing, but not enough to affect the
  // current frame or target layout.
  other_view()->set_preferred_size(gfx::Size(5, 5));
  test::RunScheduledLayout(root_view());
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->size());
  EXPECT_FALSE(child(0)->GetVisible());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_EQ(child(2)->size(), kChildViewSize);

  // Verify the event log.
  const std::vector<bool> expected_events{true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest,
       ShrinkWithinConstrainedSpace_ExpandedSpaceHasNoEffect) {
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  AnimationEventLogger logger(layout());

  // Shrink the view by hiding child views.
  child(0)->SetVisible(false);
  child(1)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(500));

  // Further unconstrain the layout before continuing.
  root_view()->SetSize({preferred.width() + 5, preferred.height()});
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(500));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->size());
  EXPECT_FALSE(child(0)->GetVisible());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_EQ(child(2)->size(), kChildViewSize);

  // Verify the event log.
  const std::vector<bool> expected_events{true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest,
       ShrinkWithinConstrainedSpace_SnapToFinalLayout) {
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  AnimationEventLogger logger(layout());

  // Shrink the view by hiding child views.
  child(0)->SetVisible(false);
  child(1)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(500));

  // Constrain the layout before continuing.
  other_view()->set_preferred_size(gfx::Size(20, 5));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->size());
  EXPECT_FALSE(child(0)->GetVisible());
  EXPECT_FALSE(child(1)->GetVisible());
  EXPECT_EQ(child(2)->size(), kChildViewSize);

  // Verify the event log.
  const std::vector<bool> expected_events{true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest,
       ShrinkWithinConstrainedSpace_SnapToConstrainedLayout) {
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  AnimationEventLogger logger(layout());

  // Shrink the view by hiding child views.
  child(0)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(500));

  // Constrain the layout before continuing.
  other_view()->set_preferred_size(gfx::Size(20, 5));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->size());
  EXPECT_FALSE(child(0)->GetVisible());
  EXPECT_EQ(child(1)->size(), kChildViewSize);
  EXPECT_FALSE(child(2)->GetVisible());

  // Verify the event log.
  const std::vector<bool> expected_events{true, false};
  EXPECT_EQ(expected_events, logger.events());
}

TEST_F(AnimatingLayoutManagerInFlexLayoutTest,
       ShrinkWithinConstrainedSpace_NoRestartOnLargerPreferredSize) {
  const gfx::Size preferred = target_layout()->GetPreferredSize(view());
  root_view()->SetSize(preferred);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());

  AnimationEventLogger logger(layout());

  // Shrink the view by hiding child views.
  child(0)->SetVisible(false);
  child(1)->SetVisible(false);
  EXPECT_TRUE(layout()->is_animating());
  animation_api()->IncrementTime(base::Milliseconds(750));

  // Constrain the layout before continuing.
  other_view()->set_preferred_size(gfx::Size(20, 5));
  child(1)->SetVisible(true);
  test::RunScheduledLayout(root_view());
  EXPECT_TRUE(layout()->is_animating());

  // Finish the animation.
  animation_api()->IncrementTime(base::Milliseconds(250));
  test::RunScheduledLayout(root_view());
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(gfx::Size(20, 20), view()->size());
  EXPECT_FALSE(child(0)->GetVisible());
  EXPECT_EQ(child(1)->size(), kChildViewSize);
  EXPECT_FALSE(child(2)->GetVisible());

  // Verify the event log.
  const std::vector<bool> expected_events{true, false};
  EXPECT_EQ(expected_events, logger.events());
}

// Test without animation.
class AnimatingLayoutManagerInFlexLayoutNoAnimationTest
    : public AnimatingLayoutManagerInFlexLayoutTest {
 public:
  AnimatingLayoutManagerInFlexLayoutNoAnimationTest()
      : AnimatingLayoutManagerInFlexLayoutTest(false) {}
};

// Regression test for crbug.com/1311708
// This test will fail without the fix made to
// AnimatingLayoutManager::GetPreferredSize().
TEST_F(AnimatingLayoutManagerInFlexLayoutNoAnimationTest, ShrinkAndGrow) {
  other_view()->SetProperty(kFlexBehaviorKey,
                            FlexSpecification(LayoutOrientation::kHorizontal,
                                              MinimumFlexSizeRule::kPreferred,
                                              MaximumFlexSizeRule::kUnbounded)
                                .WithOrder(3));
  other_view()->set_preferred_size(kChildViewSize);

  constexpr gfx::Size kFullSize = {60, 20};
  constexpr gfx::Size kReducedSize = {59, 20};
  EXPECT_EQ(kFullSize, root_view()->GetPreferredSize({}));

  root_view()->SetSize(kFullSize);
  layout()->ResetLayout();
  test::RunScheduledLayout(root_view());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 20), view()->bounds());
  EXPECT_EQ(gfx::Rect(50, 0, 10, 10), other_view()->bounds());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());

  root_view()->SetSize(kReducedSize);
  EXPECT_EQ(gfx::Rect(0, 0, 35, 20), view()->bounds());
  EXPECT_EQ(gfx::Rect(35, 0, 24, 10), other_view()->bounds());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_FALSE(child(2)->GetVisible());

  root_view()->SetSize(kFullSize);
  EXPECT_EQ(gfx::Rect(0, 0, 50, 20), view()->bounds());
  EXPECT_EQ(gfx::Rect(50, 0, 10, 10), other_view()->bounds());
  EXPECT_TRUE(child(0)->GetVisible());
  EXPECT_TRUE(child(1)->GetVisible());
  EXPECT_TRUE(child(2)->GetVisible());
}

// Realtime Tests --------------------------------------------------------------

// Test fixture for testing animations in realtime. Provides a parent view with
// an ImmediateLayoutManager so that when animation frames are triggered, the
// host view is laid out immediately. Animation durations are kept short to
// prevent tests from taking too long.
class AnimatingLayoutManagerRealtimeTest
    : public AnimatingLayoutManagerRootViewTest {
 public:
  AnimatingLayoutManagerRealtimeTest() = default;
  ~AnimatingLayoutManagerRealtimeTest() override = default;

  void SetUp() override {
    AnimatingLayoutManagerRootViewTest::SetUp();
    animation_watcher_ = std::make_unique<AnimationWatcher>(layout());
  }

  void TearDown() override {
    animation_watcher_.reset();
    AnimatingLayoutManagerRootViewTest::TearDown();
  }

  bool UseContainerTestApi() const override { return false; }

 protected:
  void InitRootView(SizeBounds bounds = SizeBounds()) {
    root_view()->SetLayoutManager(std::make_unique<ImmediateLayoutManager>(
        layout()->bounds_animation_mode(), layout()->orientation(),
        std::move(bounds)));
    layout()->EnableAnimationForTesting();
  }

  AnimationWatcher* animation_watcher() { return animation_watcher_.get(); }

 private:
  std::unique_ptr<AnimationWatcher> animation_watcher_;
};

TEST_F(AnimatingLayoutManagerRealtimeTest, TestAnimateSlide) {
  constexpr gfx::Insets kChildMargins(5);
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kUseHostBounds);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  child(1)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kPreferred,
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
  constexpr SizeBounds kSizeBounds(45, SizeBound());
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  InitRootView(kSizeBounds);
  child(0)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                          MaximumFlexSizeRule::kPreferred));
  child(0)->SetVisible(false);
  layout()->ResetLayout();
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

TEST_F(AnimatingLayoutManagerRealtimeTest, TestConstrainedSpaceDoesNotRestart) {
  constexpr gfx::Insets kChildMargins(5);
  constexpr SizeBounds kSizeBounds(45, SizeBound());
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  InitRootView(kSizeBounds);
  child(0)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                          MaximumFlexSizeRule::kPreferred));
  child(0)->SetVisible(false);
  layout()->ResetLayout();
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

  // Invalidating the host does not cause an additional layout - it knows how
  // large it can be.
  view()->InvalidateLayout();
  EXPECT_FALSE(layout()->is_animating());
  EXPECT_EQ(ending_layout.host_size, view()->size());
  EnsureLayout(ending_layout);
}

TEST_F(AnimatingLayoutManagerRealtimeTest,
       TestConstrainedSpaceRestartedAnimationSucceeds) {
  constexpr gfx::Insets kChildMargins(5);
  constexpr SizeBounds kSizeBounds(45, SizeBound());
  layout()->SetBoundsAnimationMode(
      AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
  layout()->SetAnimationDuration(kMinimumAnimationTime);
  auto* const flex_layout =
      layout()->SetTargetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
  flex_layout->SetCollapseMargins(true);
  flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
  flex_layout->SetDefault(kMarginsKey, kChildMargins);
  InitRootView(kSizeBounds);
  child(0)->SetProperty(kFlexBehaviorKey,
                        FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                          MaximumFlexSizeRule::kPreferred));
  child(0)->SetVisible(false);
  layout()->ResetLayout();
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
#if !BUILDFLAG(IS_MAC)

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
  AnimatingLayoutManagerSequenceTest() = default;
  ~AnimatingLayoutManagerSequenceTest() override = default;

  void SetUp() override {
    render_mode_lock_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);
    ViewsTestBase::SetUp();
    widget_.reset(new Widget());
    auto params = CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = {0, 0, 500, 500};
    widget_->Init(std::move(params));

    parent_view_ptr_ = std::make_unique<View>();
    parent_view_ptr_->SetLayoutManager(
        std::make_unique<ImmediateLayoutManager>());
    parent_view_ = parent_view_ptr_.get();

    layout_view_ptr_ = std::make_unique<View>();
    layout_view_ = layout_view_ptr_.get();
  }

  void TearDown() override {
    // Do before rest of tear down.
    parent_view_ = nullptr;
    layout_view_ = nullptr;
    child_view_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
    render_mode_lock_.reset();
  }

  void ConfigureLayoutView() {
    layout_view_->SetLayoutManager(std::make_unique<AnimatingLayoutManager>());
    layout_manager()->SetTweenType(gfx::Tween::Type::LINEAR);
    layout_manager()->SetAnimationDuration(kMinimumAnimationTime);
    auto* const flex_layout = layout_manager()->SetTargetLayoutManager(
        std::make_unique<FlexLayout>());
    flex_layout->SetOrientation(LayoutOrientation::kHorizontal);
    flex_layout->SetCollapseMargins(true);
    flex_layout->SetCrossAxisAlignment(LayoutAlignment::kStart);
    flex_layout->SetDefault(kMarginsKey, gfx::Insets(5));
    layout_manager()->SetBoundsAnimationMode(
        AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes);
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
    EXPECT_FALSE(layout_manager()->is_animating());
    EXPECT_EQ(gfx::Size(20, 20), layout_view_->size());
    EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child_view_->bounds());
  }

  void ExpectAnimateToLayout() {
    EXPECT_TRUE(layout_manager()->is_animating());
    AnimationWatcher animation_watcher(layout_manager());
    animation_watcher.WaitForAnimationToComplete();
    EXPECT_EQ(gfx::Size(20, 20), layout_view_->size());
    EXPECT_EQ(gfx::Rect(5, 5, 10, 10), child_view_->bounds());
  }

  AnimatingLayoutManager* layout_manager() {
    return static_cast<AnimatingLayoutManager*>(
        layout_view_->GetLayoutManager());
  }

 private:
  struct WidgetCloser {
    inline void operator()(Widget* widget) const { widget->CloseNow(); }
  };

  using WidgetAutoclosePtr = std::unique_ptr<Widget, WidgetCloser>;

  raw_ptr<View, DanglingUntriaged> child_view_ = nullptr;
  raw_ptr<View, DanglingUntriaged> parent_view_ = nullptr;
  raw_ptr<View, DanglingUntriaged> layout_view_ = nullptr;
  std::unique_ptr<View> parent_view_ptr_;
  std::unique_ptr<View> layout_view_ptr_;
  WidgetAutoclosePtr widget_;
  gfx::AnimationTestApi::RenderModeResetter render_mode_lock_;
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

#endif  // !BUILDFLAG(IS_MAC)

}  // namespace views

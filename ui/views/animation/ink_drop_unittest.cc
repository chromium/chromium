// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/ink_drop_stub.h"
#include "ui/views/animation/test/test_ink_drop_host.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_class_properties.h"

namespace views::test {

// Enumeration of all the different InkDrop types.
enum InkDropType { INK_DROP_STUB, INK_DROP_IMPL };

class InkDropTest : public testing::TestWithParam<testing::tuple<InkDropType>> {
 public:
  InkDropTest();

  InkDropTest(const InkDropTest&) = delete;
  InkDropTest& operator=(const InkDropTest&) = delete;

  ~InkDropTest() override;

 protected:
  // A dummy InkDropHost required to create an InkDrop.
  TestInkDropHost test_ink_drop_host_;

  // The InkDrop returned by the InkDropFactory test target.
  std::unique_ptr<InkDrop> ink_drop_;

 private:
  // Extracts and returns the InkDropType from the test parameters.
  InkDropType GetInkDropType() const;

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  // Required by base::Timer's.
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle>
      thread_task_runner_current_default_handle_;
};

InkDropTest::InkDropTest() : ink_drop_(nullptr) {
  zero_duration_mode_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  switch (GetInkDropType()) {
    case INK_DROP_STUB:
      ink_drop_ = std::make_unique<InkDropStub>();
      break;
    case INK_DROP_IMPL:
      ink_drop_ = std::make_unique<InkDropImpl>(
          InkDrop::Get(&test_ink_drop_host_), gfx::Size(),
          InkDropImpl::AutoHighlightMode::NONE);
      // The Timer's used by the InkDropImpl class require a
      // base::SingleThreadTaskRunner::CurrentDefaultHandle instance.
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner(
          new base::TestMockTimeTaskRunner);
      thread_task_runner_current_default_handle_ =
          std::make_unique<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
              task_runner);
      break;
  }
}

InkDropTest::~InkDropTest() = default;

InkDropType InkDropTest::GetInkDropType() const {
  return testing::get<0>(GetParam());
}

// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_SUITE_P(All,
                         InkDropTest,
                         testing::Values(INK_DROP_STUB, INK_DROP_IMPL));

TEST_P(InkDropTest,
       VerifyInkDropLayersRemovedAfterDestructionWhenRippleIsActive) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_.reset();
  EXPECT_EQ(0, test_ink_drop_host_.num_ink_drop_layers());
}

TEST_P(InkDropTest, StateIsHiddenInitially) {
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropTest, TypicalQuickAction) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ACTION_TRIGGERED);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropTest, CancelQuickAction) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropTest, TypicalSlowAction) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ALTERNATE_ACTION_TRIGGERED);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropTest, CancelSlowAction) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::HIDDEN);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropTest, TypicalQuickActivated) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ACTIVATED);
  ink_drop_->AnimateToState(InkDropState::DEACTIVATED);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

TEST_P(InkDropTest, TypicalSlowActivated) {
  ink_drop_->AnimateToState(InkDropState::ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING);
  ink_drop_->AnimateToState(InkDropState::ACTIVATED);
  ink_drop_->AnimateToState(InkDropState::DEACTIVATED);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop_->GetTargetInkDropState());
}

using InkDropContainerViewTest = ViewsTestBase;

// Ensure that the inkdrop tracks its parent View's bounds, even if the parent
// has its own layout manager.
TEST_F(InkDropContainerViewTest, MatchesParentBounds) {
  const auto widget = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* const contents = widget->SetContentsView(std::make_unique<View>());
  auto* const child1 = contents->AddChildView(
      std::make_unique<StaticSizedView>(gfx::Size(20, 20)));
  child1->set_minimum_size(gfx::Size(10, 10));
  auto* const child2 = contents->AddChildView(
      std::make_unique<StaticSizedView>(gfx::Size(20, 20)));
  child2->set_minimum_size(gfx::Size(10, 10));
  auto* const layout =
      contents->SetLayoutManager(std::make_unique<FlexLayout>());
  layout->SetDefault(kFlexBehaviorKey,
                     FlexSpecification(LayoutOrientation::kHorizontal,
                                       MinimumFlexSizeRule::kScaleToMinimum,
                                       MaximumFlexSizeRule::kPreferred));
  auto* const inkdrop_container =
      contents->AddChildView(std::make_unique<InkDropContainerView>());
  inkdrop_container->SetAutoMatchParentBounds(true);

  // Start near the default size for the layout.
  widget->SetBounds({20, 20, 40, 20});
  widget->LayoutRootViewIfNecessary();

  // Ensure that the inkdrop container has (a) been laid out, and (b) has the
  // expected bounds.
  EXPECT_EQ(gfx::Point(0, 0), inkdrop_container->origin());
  EXPECT_EQ(contents->size(), inkdrop_container->size());
  EXPECT_FALSE(inkdrop_container->needs_layout());

  // Shrink the widget and ensure the inkdrop container shrinks too.
  widget->SetBounds({20, 20, 20, 20});
  widget->LayoutRootViewIfNecessary();
  EXPECT_EQ(gfx::Point(0, 0), inkdrop_container->origin());
  EXPECT_EQ(contents->size(), inkdrop_container->size());
  EXPECT_FALSE(inkdrop_container->needs_layout());
}

}  // namespace views::test

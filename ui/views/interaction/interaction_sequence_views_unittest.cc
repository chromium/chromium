// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_sequence_views.h"

// This suite contains tests which integrate the functionality of
// ui::InteractionSequence with Views elements like Widgets and buttons.
// Similar suites should be created for other platforms.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kContentsElementID);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementID);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementID2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementID3);
const char kElementName[] = "ElementName";

}  // namespace

class InteractionSequenceViewsTest
    : public ViewsTestBase,
      public testing::WithParamInterface<
          ui::InteractionSequence::StepStartMode> {
 public:
  InteractionSequenceViewsTest() = default;
  ~InteractionSequenceViewsTest() override = default;

  static View* ElementToView(ui::TrackedElement* element) {
    return element ? element->AsA<TrackedElementViews>()->view() : nullptr;
  }

  static ui::TrackedElement* ViewToElement(View* view) {
    return view ? ElementTrackerViews::GetInstance()->GetElementForView(view)
                : nullptr;
  }

  std::unique_ptr<Widget> CreateWidget() {
    auto widget = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget->Init(std::move(params));
    auto* contents = widget->SetContentsView(std::make_unique<View>());
    auto* layout = contents->SetLayoutManager(std::make_unique<FlexLayout>());
    layout->SetOrientation(LayoutOrientation::kHorizontal);
    layout->SetDefault(kFlexBehaviorKey,
                       FlexSpecification(MinimumFlexSizeRule::kPreferred,
                                         MaximumFlexSizeRule::kUnbounded));
    test::WidgetVisibleWaiter visible_waiter(widget.get());
    widget->Show();
    visible_waiter.Wait();
    return widget;
  }

  void ShowBubble(ui::ElementIdentifier id) {
    auto delegate = std::make_unique<BubbleDialogDelegateView>(
        contents(), BubbleBorder::Arrow::TOP_LEFT);
    label_button_ = delegate->AddChildView(std::make_unique<LabelButton>());
    label_button_->SetProperty(kElementIdentifierKey, id);
    no_id_view_ = delegate->AddChildView(std::make_unique<LabelButton>());
    bubble_widget_ =
        BubbleDialogDelegateView::CreateBubble(std::move(delegate));
    test::WidgetVisibleWaiter visible_waiter(bubble_widget_);
    bubble_widget_->Show();
    visible_waiter.Wait();
  }

  void CloseBubble() {
    DCHECK(bubble_widget_);
    label_button_ = nullptr;
    no_id_view_ = nullptr;
    bubble_widget_.ExtractAsDangling()->CloseNow();
  }

  void Activate(View* view) {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(
        ElementTrackerViews::GetInstance()->GetElementForView(view));
  }

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateWidget();
    contents()->SetProperty(kElementIdentifierKey, kContentsElementID);
  }

  void TearDown() override {
    if (bubble_widget_) {
      CloseBubble();
    }
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  ui::InteractionSequence::Builder Builder() const {
    return std::move(
        ui::InteractionSequence::Builder().SetDefaultStepStartMode(GetParam()));
  }

 protected:
  ui::ElementContext context() const {
    return ui::ElementContext(widget_.get());
  }

  View* contents() { return widget_->GetContentsView(); }

  std::unique_ptr<Widget> widget_;
  raw_ptr<Widget> bubble_widget_ = nullptr;
  raw_ptr<LabelButton> label_button_ = nullptr;
  raw_ptr<LabelButton> no_id_view_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    InteractionSequenceViewsTest,
    testing::Values(ui::InteractionSequence::StepStartMode::kImmediate,
                    ui::InteractionSequence::StepStartMode::kAsynchronous),
    [](const testing::TestParamInfo<ui::InteractionSequence::StepStartMode>&
           mode) {
      std::ostringstream oss;
      oss << mode.param;
      return oss.str();
    });

TEST_P(InteractionSequenceViewsTest, DestructWithInitialViewAborts) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  auto* const starting_view =
      contents()->AddChildView(std::make_unique<View>());
  starting_view->SetProperty(kElementIdentifierKey, kTestElementID);
  auto sequence =
      Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(starting_view))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_ASYNC_CALL_IN_SCOPE(aborted, Run,
                             contents()->RemoveChildViewT(starting_view));
}

TEST_P(InteractionSequenceViewsTest, DestructWithInitialViewBeforeStartAborts) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  auto* const starting_view =
      contents()->AddChildView(std::make_unique<View>());
  starting_view->SetProperty(kElementIdentifierKey, kTestElementID);
  auto sequence =
      Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(starting_view))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  contents()->RemoveChildViewT(starting_view);
  EXPECT_ASYNC_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST_P(InteractionSequenceViewsTest, WrongWithInitialViewDoesNotStartSequence) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  auto* const starting_view =
      contents()->AddChildView(std::make_unique<View>());
  starting_view->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* const other_view = contents()->AddChildView(std::make_unique<View>());
  other_view->SetProperty(kElementIdentifierKey, kTestElementID);
  auto sequence =
      Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(starting_view))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  starting_view->SetVisible(false);
  EXPECT_ASYNC_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST_P(InteractionSequenceViewsTest,
       SequenceNotCanceledDueToViewDestroyedIfRequirementChanged) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback,
                         step2_start);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepEndCallback, step2_end);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback,
                         step3_start);
  auto* const starting_view =
      contents()->AddChildView(std::make_unique<View>());
  starting_view->SetProperty(kElementIdentifierKey, kTestElementID);
  auto sequence =
      Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(starting_view))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID2)
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID2)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       // Specify that this step doesn't abort on the view
                       // becoming hidden.
                       .SetMustRemainVisible(false)
                       .SetStartCallback(step2_start.Get())
                       .SetEndCallback(step2_end.Get())
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID3)
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetStartCallback(step3_start.Get())
                       .Build())
          .Build();
  sequence->Start();
  auto* const second_view = contents()->AddChildView(std::make_unique<View>());
  second_view->SetProperty(kElementIdentifierKey, kTestElementID2);
  auto* const third_view = contents()->AddChildView(std::make_unique<View>());
  third_view->SetProperty(kElementIdentifierKey, kTestElementID3);
  third_view->SetVisible(false);

  // Simulate the view being activated to do the second step.
  EXPECT_ASYNC_CALL_IN_SCOPE(step2_start,
                             Run(sequence.get(), ViewToElement(second_view)),
                             Activate(second_view));

  // Destroying the second view should NOT break the sequence.
  contents()->RemoveChildViewT(second_view);

  // Showing the third view at this point continues the sequence.
  EXPECT_ASYNC_CALLS_IN_SCOPE_3(step2_end, Run, step3_start, Run, completed,
                                Run, third_view->SetVisible(true));
}

// The tests are failing on debug swiftshader on arm64, see
// https://ci.chromium.org/ui/p/chromium/builders/ci/fuchsia-fyi-arm64-dbg/9234/overview
// TODO(crbug.com/42050042): Re-enable the tests once we get rid of swiftshader.
#if BUILDFLAG(IS_FUCHSIA) && !defined(NDEBUG) && defined(ARCH_CPU_ARM64)
#define MAYBE_TransitionToBubble DISABLED_TransitionToBubble
#else
#define MAYBE_TransitionToBubble TransitionToBubble
#endif
TEST_P(InteractionSequenceViewsTest, MAYBE_TransitionToBubble) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step3);
  auto sequence =
      Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents()))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step.Get())
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID2)
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetStartCallback(step2.Get())
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID2)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step3.Get())
                       .Build())
          .Build();
  auto* const button = contents()->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowBubble,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  button->SetAccessibleName(u"Button");
  sequence->Start();

  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      step, Run, step2, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      step3, Run, completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          label_button_));
}

// The tests are failing on debug swiftshader on arm64, see
// https://ci.chromium.org/ui/p/chromium/builders/ci/fuchsia-fyi-arm64-dbg/9234/overview
// TODO(crbug.com/42050042): Re-enable the tests once we get rid of swiftshader.
#if BUILDFLAG(IS_FUCHSIA) && !defined(NDEBUG) && defined(ARCH_CPU_ARM64)
#define MAYBE_TransitionToBubbleThenAbort DISABLED_TransitionToBubbleThenAbort
#else
#define MAYBE_TransitionToBubbleThenAbort TransitionToBubbleThenAbort
#endif
TEST_P(InteractionSequenceViewsTest, MAYBE_TransitionToBubbleThenAbort) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step3);
  auto sequence =
      Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents()))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step.Get())
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID2)
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetStartCallback(step2.Get())
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID2)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step3.Get())
                       .Build())
          .Build();
  auto* const button = contents()->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowBubble,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  button->SetAccessibleName(u"Button");
  sequence->Start();

  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      step, Run, step2, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_ASYNC_CALL_IN_SCOPE(aborted, Run, CloseBubble());
}

// NameView tests:

// The tests are failing on debug swiftshader on arm64, see
// https://ci.chromium.org/ui/p/chromium/builders/ci/fuchsia-fyi-arm64-dbg/9234/overview
// TODO(crbug.com/42050042): Re-enable the tests once we get rid of swiftshader.
#if BUILDFLAG(IS_FUCHSIA) && !defined(NDEBUG) && defined(ARCH_CPU_ARM64)
#define MAYBE_NameView_NameViewWithIdentifier \
  DISABLED_NameView_NameViewWithIdentifier
#else
#define MAYBE_NameView_NameViewWithIdentifier NameView_NameViewWithIdentifier
#endif
TEST_P(InteractionSequenceViewsTest, MAYBE_NameView_NameViewWithIdentifier) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step3);
  auto step2 = base::BindLambdaForTesting([&](ui::InteractionSequence* sequence,
                                              ui::TrackedElement* element) {
    EXPECT_EQ(label_button_, element->AsA<TrackedElementViews>()->view());
    InteractionSequenceViews::NameView(sequence, label_button_, kElementName);
  });
  auto sequence =
      Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents()))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step.Get())
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID2)
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetStartCallback(std::move(step2))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementName(kElementName)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step3.Get())
                       .Build())
          .Build();
  auto* const button = contents()->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowBubble,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  button->SetAccessibleName(u"Button");
  sequence->Start();

  EXPECT_ASYNC_CALL_IN_SCOPE(
      step, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      step3, Run, completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          label_button_));
}

// The tests are failing on debug swiftshader on arm64, see
// https://ci.chromium.org/ui/p/chromium/builders/ci/fuchsia-fyi-arm64-dbg/9234/overview
// TODO(crbug.com/42050042): Re-enable the tests once we get rid of swiftshader.
#if BUILDFLAG(IS_FUCHSIA) && !defined(NDEBUG) && defined(ARCH_CPU_ARM64)
#define MAYBE_NameView_NameViewWithNoIdentifier \
  DISABLED_NameView_NameViewWithNoIdentifier
#else
#define MAYBE_NameView_NameViewWithNoIdentifier \
  NameView_NameViewWithNoIdentifier
#endif
TEST_P(InteractionSequenceViewsTest, MAYBE_NameView_NameViewWithNoIdentifier) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step3);
  auto step2 = base::BindLambdaForTesting(
      [&](ui::InteractionSequence* sequence, ui::TrackedElement* element) {
        EXPECT_EQ(label_button_, element->AsA<TrackedElementViews>()->view());
        InteractionSequenceViews::NameView(sequence, no_id_view_, kElementName);
      });
  auto sequence =
      Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents()))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step.Get())
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID2)
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetStartCallback(std::move(step2))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementName(kElementName)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .SetStartCallback(step3.Get())
                       .Build())
          .Build();
  auto* const button = contents()->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowBubble,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  button->SetAccessibleName(u"Button");
  sequence->Start();

  EXPECT_ASYNC_CALL_IN_SCOPE(
      step, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      step3, Run, completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(no_id_view_));
}

}  // namespace views

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_sequence_views.h"

// This suite contains tests which integrate the functionality of
// ui::InteractionSequence with Views elements like Widgets and menus.
// Similar suites should be created for other platforms.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
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

DECLARE_ELEMENT_IDENTIFIER_VALUE(kContentsElementID);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTestElementID);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTestElementID2);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTestElementID3);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kContentsElementID);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kTestElementID);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kTestElementID2);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kTestElementID3);
const char16_t kMenuItem1[] = u"Menu item";
const char16_t kMenuItem2[] = u"Menu item 2";
constexpr int kMenuID1 = 1;
constexpr int kMenuID2 = 2;
const char kElementName[] = "ElementName";

}  // namespace

class InteractionSequenceViewsTest : public ViewsTestBase {
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
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
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

  void ShowMenu(ui::ElementIdentifier id) {
    CreateAndRunMenu(id);

    menu_element_ = ui::SafeElementReference(
        ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
            id, ElementTrackerViews::GetContextForView(contents_)));
    Widget* const menu_widget = ElementToView(menu_element_.get())->GetWidget();
    test::WidgetVisibleWaiter visible_waiter(menu_widget);
    visible_waiter.Wait();
  }

  void CloseMenu() {
    menu_runner_.reset();
    menu_model_.reset();
    menu_element_ = ui::SafeElementReference();
  }

  void ShowBubble(ui::ElementIdentifier id) {
    auto delegate = std::make_unique<BubbleDialogDelegateView>(
        contents_, BubbleBorder::Arrow::TOP_LEFT);
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
    bubble_widget_->CloseNow();
    bubble_widget_ = nullptr;
    label_button_ = nullptr;
  }

  void Activate(View* view) {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(
        ElementTrackerViews::GetInstance()->GetElementForView(view));
  }

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateWidget();
    contents_ = widget_->GetContentsView();
    contents_->SetProperty(kElementIdentifierKey, kContentsElementID);
  }

  void TearDown() override {
    if (bubble_widget_)
      CloseBubble();
    if (menu_runner_)
      CloseMenu();
    widget_.reset();
    contents_ = nullptr;
    ViewsTestBase::TearDown();
  }

 protected:
  ui::ElementContext context() const {
    return ui::ElementContext(widget_.get());
  }

  virtual void CreateAndRunMenu(ui::ElementIdentifier id) {
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    menu_model_->AddItem(kMenuID1, kMenuItem1);
    menu_model_->AddItem(kMenuID2, kMenuItem2);
    menu_model_->SetElementIdentifierAt(
        menu_model_->GetIndexOfCommandId(kMenuID2).value(), id);

    menu_runner_ =
        std::make_unique<MenuRunner>(menu_model_.get(), MenuRunner::NO_FLAGS);
    menu_runner_->RunMenuAt(
        widget_.get(), nullptr, gfx::Rect(gfx::Point(), gfx::Size(200, 200)),
        MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE);
  }

  std::unique_ptr<Widget> widget_;
  raw_ptr<View> contents_ = nullptr;
  raw_ptr<Widget> bubble_widget_ = nullptr;
  raw_ptr<LabelButton> label_button_ = nullptr;
  raw_ptr<LabelButton> no_id_view_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<MenuRunner> menu_runner_;
  ui::SafeElementReference menu_element_;
};

TEST_F(InteractionSequenceViewsTest, DestructWithInitialViewAborts) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  auto* const starting_view = contents_->AddChildView(std::make_unique<View>());
  starting_view->SetProperty(kElementIdentifierKey, kTestElementID);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(starting_view))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  sequence->Start();
  EXPECT_CALL_IN_SCOPE(aborted, Run,
                       contents_->RemoveChildViewT(starting_view));
}

TEST_F(InteractionSequenceViewsTest, DestructWithInitialViewBeforeStartAborts) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  auto* const starting_view = contents_->AddChildView(std::make_unique<View>());
  starting_view->SetProperty(kElementIdentifierKey, kTestElementID);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(starting_view))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  contents_->RemoveChildViewT(starting_view);
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST_F(InteractionSequenceViewsTest, WrongWithInitialViewDoesNotStartSequence) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  auto* const starting_view = contents_->AddChildView(std::make_unique<View>());
  starting_view->SetProperty(kElementIdentifierKey, kTestElementID);
  auto* const other_view = contents_->AddChildView(std::make_unique<View>());
  other_view->SetProperty(kElementIdentifierKey, kTestElementID);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(starting_view))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kTestElementID)
                       .SetType(ui::InteractionSequence::StepType::kActivated)
                       .Build())
          .Build();
  starting_view->SetVisible(false);
  EXPECT_CALL_IN_SCOPE(aborted, Run, sequence->Start());
}

TEST_F(InteractionSequenceViewsTest,
       SequenceNotCanceledDueToViewDestroyedIfRequirementChanged) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback,
                         step2_start);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepEndCallback, step2_end);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback,
                         step3_start);
  auto* const starting_view = contents_->AddChildView(std::make_unique<View>());
  starting_view->SetProperty(kElementIdentifierKey, kTestElementID);
  auto sequence =
      ui::InteractionSequence::Builder()
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
  auto* const second_view = contents_->AddChildView(std::make_unique<View>());
  second_view->SetProperty(kElementIdentifierKey, kTestElementID2);
  auto* const third_view = contents_->AddChildView(std::make_unique<View>());
  third_view->SetProperty(kElementIdentifierKey, kTestElementID3);
  third_view->SetVisible(false);

  // Simulate the view being activated to do the second step.
  EXPECT_CALL_IN_SCOPE(step2_start,
                       Run(sequence.get(), ViewToElement(second_view)),
                       Activate(second_view));

  // Destroying the second view should NOT break the sequence.
  contents_->RemoveChildViewT(second_view);

  // Showing the third view at this point continues the sequence.
  EXPECT_CALLS_IN_SCOPE_3(step2_end, Run, step3_start, Run, completed, Run,
                          third_view->SetVisible(true));
}

TEST_F(InteractionSequenceViewsTest, TransitionToBubble) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step3);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents_))
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
  auto* const button = contents_->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowBubble,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  sequence->Start();

  EXPECT_CALLS_IN_SCOPE_2(
      step, Run, step2, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_CALLS_IN_SCOPE_2(
      step3, Run, completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          label_button_));
}

TEST_F(InteractionSequenceViewsTest, TransitionToBubbleThenAbort) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step3);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents_))
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
  auto* const button = contents_->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowBubble,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  sequence->Start();

  EXPECT_CALLS_IN_SCOPE_2(
      step, Run, step2, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_CALL_IN_SCOPE(aborted, Run, CloseBubble());
}

TEST_F(InteractionSequenceViewsTest, TransitionToMenuAndViewMenuItem) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step2);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents_))
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
          .Build();

  auto* const button = contents_->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowMenu,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  sequence->Start();

  EXPECT_CALLS_IN_SCOPE_3(
      step, Run, step2, Run, completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));
}

TEST_F(InteractionSequenceViewsTest, TransitionToMenuThenCloseMenuToCancel) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step3);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents_))
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
  auto* const button = contents_->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowMenu,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  sequence->Start();

  EXPECT_CALLS_IN_SCOPE_2(
      step, Run, step2, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_CALL_IN_SCOPE(aborted, Run, CloseMenu());
}

// Menu button uses different event-handling architecture than standard Button,
// so test it separately here.
TEST_F(InteractionSequenceViewsTest, TransitionToMenuWithMenuButton) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step2);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents_))
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
          .Build();

  auto* const button = contents_->AddChildView(
      std::make_unique<MenuButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowMenu,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  sequence->Start();

  EXPECT_CALLS_IN_SCOPE_3(
      step, Run, step2, Run, completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));
}

TEST_F(InteractionSequenceViewsTest, TransitionToMenuAndActivateMenuItem) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step3);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents_))
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
  auto* const button = contents_->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowMenu,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  sequence->Start();

  EXPECT_CALLS_IN_SCOPE_2(
      step, Run, step2, Run,
      test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_CALLS_IN_SCOPE_2(step3, Run, completed, Run, {
    ui::test::InteractionTestUtil test_util;
    test_util.AddSimulator(
        std::make_unique<test::InteractionTestUtilSimulatorViews>());
    test_util.SelectMenuItem(menu_element_.get());
  });
}

TEST_F(InteractionSequenceViewsTest, TransitionOnKeyboardMenuActivation) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step2);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::StepStartCallback, step3);
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents_))
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
  auto* const button = contents_->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowMenu,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  sequence->Start();

  EXPECT_CALLS_IN_SCOPE_2(
      step, Run, step2, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_CALLS_IN_SCOPE_2(step3, Run, completed, Run, {
    ui::test::EventGenerator generator(GetContext(),
                                       widget_->GetNativeWindow());
    generator.PressKey(ui::VKEY_DOWN, 0);
    generator.PressKey(ui::VKEY_DOWN, 0);
    generator.PressKey(ui::VKEY_RETURN, 0);
  });
}

// NameView tests:

TEST_F(InteractionSequenceViewsTest, NameView_NameViewWithIdentifier) {
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
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents_))
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
  auto* const button = contents_->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowBubble,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  sequence->Start();

  EXPECT_CALL_IN_SCOPE(
      step, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_CALLS_IN_SCOPE_2(
      step3, Run, completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          label_button_));
}

TEST_F(InteractionSequenceViewsTest, NameView_NameViewWithNoIdentifier) {
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
      ui::InteractionSequence::Builder()
          .SetAbortedCallback(aborted.Get())
          .SetCompletedCallback(completed.Get())
          .AddStep(InteractionSequenceViews::WithInitialView(contents_))
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
  auto* const button = contents_->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(
          base::BindRepeating(&InteractionSequenceViewsTest::ShowBubble,
                              base::Unretained(this), kTestElementID2))));
  button->SetProperty(kElementIdentifierKey, kTestElementID);
  sequence->Start();

  EXPECT_CALL_IN_SCOPE(
      step, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(button));

  EXPECT_CALLS_IN_SCOPE_2(
      step3, Run, completed, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(no_id_view_));
}

}  // namespace views

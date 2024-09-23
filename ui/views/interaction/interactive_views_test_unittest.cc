// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interactive_views_test.h"

#include <functional>
#include <memory>
#include <string>

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace views::test {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kButtonsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kButton1Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabbedPaneId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kScrollChild1Id);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kScrollChild2Id);
constexpr char16_t kButton1Caption[] = u"Button 1";
constexpr char16_t kButton2Caption[] = u"Button 2";
constexpr char16_t kTab1Title[] = u"Tab 1";
constexpr char16_t kTab2Title[] = u"Tab 2";
constexpr char16_t kTab3Title[] = u"Tab 3";
constexpr char16_t kTab1Contents[] = u"Tab 1 Contents";
constexpr char16_t kTab2Contents[] = u"Tab 2 Contents";
constexpr char16_t kTab3Contents[] = u"Tab 3 Contents";
constexpr char kViewName[] = "Named View";
constexpr char kViewName2[] = "Second Named View";
}  // namespace

class InteractiveViewsTestTest : public InteractiveViewsTest {
 public:
  InteractiveViewsTestTest() = default;
  ~InteractiveViewsTestTest() override = default;

  void SetUp() override {
    InteractiveViewsTest::SetUp();

    // Set up the Views hierarchy to use for the tests.
    auto contents =
        Builder<FlexLayoutView>()
            .SetProperty(kElementIdentifierKey, kContentsId)
            .SetOrientation(LayoutOrientation::kVertical)
            .AddChildren(
                Builder<TabbedPane>()
                    .CopyAddressTo(&tabs_)
                    .SetProperty(kElementIdentifierKey, kTabbedPaneId)
                    .AddTab(kTab1Title, std::make_unique<Label>(kTab1Contents))
                    .AddTab(kTab2Title, std::make_unique<Label>(kTab2Contents))
                    .AddTab(kTab3Title, std::make_unique<Label>(kTab3Contents)),
                Builder<FlexLayoutView>()
                    .SetProperty(kElementIdentifierKey, kButtonsId)
                    .SetOrientation(LayoutOrientation::kHorizontal)
                    .AddChildren(
                        Builder<LabelButton>()
                            .CopyAddressTo(&button1_)
                            .SetProperty(kElementIdentifierKey, kButton1Id)
                            .SetText(kButton1Caption)
                            .SetCallback(button1_callback_.Get()),
                        Builder<LabelButton>()
                            .CopyAddressTo(&button2_)
                            .SetText(kButton2Caption)
                            .SetCallback(button2_callback_.Get())),
                Builder<ScrollView>()
                    .CopyAddressTo(&scroll_)
                    .SetPreferredSize(gfx::Size(100, 90))
                    .SetVerticalScrollBarMode(
                        ScrollView::ScrollBarMode::kEnabled)
                    .SetContents(
                        Builder<FlexLayoutView>()
                            .SetOrientation(LayoutOrientation::kVertical)
                            .SetSize(gfx::Size(100, 200))
                            .AddChildren(
                                Builder<View>()
                                    .SetProperty(kElementIdentifierKey,
                                                 kScrollChild1Id)
                                    .SetPreferredSize(gfx::Size(100, 100)),
                                Builder<View>()
                                    .SetProperty(kElementIdentifierKey,
                                                 kScrollChild2Id)
                                    .SetPreferredSize(gfx::Size(100, 100)))));

    // Create and show the test widget.
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->SetContentsView(std::move(contents).Build());
    WidgetVisibleWaiter waiter(widget_.get());
    widget_->Show();
    waiter.Wait();
    widget_->LayoutRootViewIfNecessary();

    // This is required before RunTestSequence() can be called.
    SetContextWidget(widget_.get());
  }

  void TearDown() override {
    SetContextWidget(nullptr);
    tabs_ = nullptr;
    button1_ = nullptr;
    button2_ = nullptr;
    scroll_ = nullptr;
    widget_.reset();
    InteractiveViewsTest::TearDown();
  }

  static void DoPost(base::OnceClosure closure) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
  }

  auto Post(base::OnceClosure closure) {
    return Do(base::BindOnce(
        [](base::OnceClosure closure) { DoPost(std::move(closure)); },
        std::move(closure)));
  }

 protected:
  using ButtonCallbackMock = testing::StrictMock<
      base::MockCallback<Button::PressedCallback::Callback>>;

  std::unique_ptr<Widget> widget_;
  raw_ptr<TabbedPane> tabs_;
  raw_ptr<LabelButton> button1_;
  raw_ptr<LabelButton> button2_;
  raw_ptr<ScrollView> scroll_;
  ButtonCallbackMock button1_callback_;
  ButtonCallbackMock button2_callback_;
};

TEST_F(InteractiveViewsTestTest, WithView) {
  RunTestSequence(WithView(kButton1Id, base::BindOnce([](LabelButton* button) {
                             EXPECT_TRUE(button->GetVisible());
                           })),
                  // Check version with arbitrary return value and matcher.
                  WithView(kTabbedPaneId, base::BindOnce([](TabbedPane* tabs) {
                             EXPECT_EQ(3U, tabs->GetTabCount());
                           })));
}

TEST_F(InteractiveViewsTestTest, CheckView) {
  RunTestSequence(
      // Check version with no matcher and only boolean return value.
      CheckView(kButton1Id, base::BindOnce([](LabelButton* button) {
                  return button->GetVisible();
                })),
      // Check version with arbitrary return value and matcher.
      CheckView(kTabbedPaneId, base::BindOnce([](TabbedPane* tabs) {
                  return tabs->GetTabCount();
                }),
                testing::Gt(2U)));
}

TEST_F(InteractiveViewsTestTest, CheckViewFails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          // Check version with no matcher and only boolean return value.
          CheckView(kButton1Id, base::BindOnce([](LabelButton* button) {
                      return !button->GetVisible();
                    }))));
}

TEST_F(InteractiveViewsTestTest, CheckViewProperty) {
  RunTestSequence(
      CheckViewProperty(kButton1Id, &LabelButton::GetText,
                        // Implicit creation of an equality matcher.
                        kButton1Caption),
      CheckViewProperty(kTabbedPaneId, &TabbedPane::GetSelectedTabIndex,
                        // Explicit creation of an inequality matcher.
                        testing::Ne(1U)));
}

TEST_F(InteractiveViewsTestTest, CheckViewPropertyFails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(CheckViewProperty(
          kTabbedPaneId, &TabbedPane::GetSelectedTabIndex, testing::Eq(1U))));
}

TEST_F(InteractiveViewsTestTest, WaitForViewProperty_AlreadyTrue) {
  RunTestSequence(WaitForViewProperty(kButton1Id, View, Enabled, true));
}

TEST_F(InteractiveViewsTestTest, WaitForViewProperty_BecomesTrue) {
  button1_->SetEnabled(false);
  DoPost(base::BindLambdaForTesting([this]() { button1_->SetEnabled(true); }));
  RunTestSequence(WaitForViewProperty(kButton1Id, View, Enabled, true));
}

TEST_F(InteractiveViewsTestTest, PollView) {
  using Observer = PollingViewObserver<std::u16string, LabelButton>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(Observer, kButtonTextState);
  DoPost(base::BindLambdaForTesting(
      [this]() { button1_->SetText(kButton2Caption); }));
  RunTestSequence(PollView(kButtonTextState, kButton1Id,
                           [](const LabelButton* b) -> std::u16string {
                             return b->GetText();
                           }),
                  WaitForState(kButtonTextState, kButton2Caption));
}

TEST_F(InteractiveViewsTestTest, PollViewProperty) {
  using Observer = PollingViewPropertyObserver<std::u16string, LabelButton>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(Observer, kButtonTextState);
  DoPost(base::BindLambdaForTesting(
      [this]() { button1_->SetText(kButton2Caption); }));
  RunTestSequence(
      PollViewProperty(kButtonTextState, kButton1Id, &LabelButton::GetText),
      WaitForState(kButtonTextState, kButton2Caption));
}

TEST_F(InteractiveViewsTestTest, WaitForViewPropertyFails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  button1_->SetEnabled(false);
  DoPost(base::BindLambdaForTesting([this]() { button1_->SetVisible(false); }));
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(WaitForViewProperty(kButton1Id, View, Enabled, true)));
}

TEST_F(InteractiveViewsTestTest, WaitForViewPropertyInParallel) {
  button1_->SetEnabled(false);
  tabs_->SetEnabled(false);
  RunTestSequence(InParallel(
      Steps(
          // These have to be inside the subsequences because there's an
          // implicit flush before a subsequence starts; if we queued them
          // all up ahead of time we wouldn't accurately be testing the
          // multiple state change reactions (they'd already be true).
          Post(base::BindLambdaForTesting(
              [this]() { tabs_->SetEnabled(true); })),
          WaitForViewProperty(kButton1Id, View, Enabled, true),
          Post(base::BindLambdaForTesting([this]() { button1_->SetID(998); })),
          WaitForViewProperty(kButton1Id, View, ID, 998)),
      Steps(Post(base::BindLambdaForTesting(
                [this]() { button1_->SetEnabled(true); })),
            WaitForViewProperty(kTabbedPaneId, View, Enabled, true),
            Post(base::BindLambdaForTesting([this]() { tabs_->SetID(999); })),
            WaitForViewProperty(kTabbedPaneId, View, ID, 999))));
}

TEST_F(InteractiveViewsTestTest, NameViewAbsoluteValue) {
  RunTestSequence(
      NameView(kViewName, button2_.get()),
      WithElement(kViewName,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    EXPECT_EQ(button2_.get(), AsView<LabelButton>(el));
                  })));
}

TEST_F(InteractiveViewsTestTest, NameViewAbsoluteDeferred) {
  View* view = nullptr;
  RunTestSequence(
      Do(base::BindLambdaForTesting([&]() { view = button2_.get(); })),
      NameView(kViewName, std::ref(view)),
      WithElement(kViewName,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    EXPECT_EQ(view, AsView(el));
                  })));
}

TEST_F(InteractiveViewsTestTest, NameViewAbsoluteCallback) {
  RunTestSequence(
      NameView(kViewName, base::BindLambdaForTesting(
                              [&]() -> View* { return button2_.get(); })),
      WithElement(kViewName,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    EXPECT_EQ(button2_.get(), AsView<LabelButton>(el));
                  })));
}

TEST_F(InteractiveViewsTestTest, NameChildViewByIndex) {
  RunTestSequence(
      NameChildView(kButtonsId, kViewName, 1U),
      WithElement(kViewName,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* const button = AsView<LabelButton>(el);
                    EXPECT_EQ(button2_.get(), button);
                    EXPECT_EQ(1U, button->parent()->GetIndexOf(button));
                  })));
}

TEST_F(InteractiveViewsTestTest, NameChildViewByFilter) {
  EXPECT_CALL_IN_SCOPE(
      button2_callback_, Run,
      RunTestSequence(
          NameChildView(
              kButtonsId, kViewName, base::BindRepeating([](const View* view) {
                const auto* const button = AsViewClass<LabelButton>(view);
                return button && button->GetText() == kButton2Caption;
              })),
          PressButton(kViewName, InputType::kKeyboard)));
}

TEST_F(InteractiveViewsTestTest, NameDescendantView) {
  EXPECT_CALL_IN_SCOPE(
      button1_callback_, Run,
      RunTestSequence(NameDescendantView(
                          kContentsId, kViewName,
                          base::BindRepeating([&](const View* view) {
                            return view->GetProperty(kElementIdentifierKey) ==
                                   kButton1Id;
                          })),
                      PressButton(kViewName, InputType::kMouse)));
}

TEST_F(InteractiveViewsTestTest, NameViewRelative) {
  RunTestSequence(
      SelectTab(kTabbedPaneId, 1U, InputType::kTouch),
      NameViewRelative(kTabbedPaneId, kViewName,
                       base::BindRepeating([&](TabbedPane* tabs) {
                         return tabs->GetTabAt(1U)->contents();
                       })),
      WithElement(kViewName,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    EXPECT_EQ(kTab2Contents, AsView<Label>(el)->GetText());
                  })));
}

TEST_F(InteractiveViewsTestTest, NameChildViewFails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          NameChildView(
              kButtonsId, kViewName, base::BindRepeating([](const View* view) {
                const auto* const button = AsViewClass<LabelButton>(view);
                return button && button->GetText() ==
                                     u"This is not a valid button caption.";
              })),
          PressButton(kViewName, InputType::kKeyboard)));
}

TEST_F(InteractiveViewsTestTest, NameChildViewByTypeAndIndex) {
  EXPECT_CALLS_IN_SCOPE_2(
      button1_callback_, Run, button2_callback_, Run,
      RunTestSequence(
          NameChildViewByType<views::LabelButton>(kButtonsId, kViewName),
          NameChildViewByType<views::LabelButton>(kButtonsId, kViewName2, 1),
          PressButton(kViewName), PressButton(kViewName2)));
}

TEST_F(InteractiveViewsTestTest, NameDescendantViewByTypeAndIndex) {
  RunTestSequence(
      NameDescendantViewByType<views::TabbedPaneTab>(kContentsId, kViewName),
      NameDescendantViewByType<views::TabbedPaneTab>(kContentsId, kViewName2,
                                                     2),
      CheckViewProperty(kViewName, &views::TabbedPaneTab::GetTitleText,
                        kTab1Title),
      CheckViewProperty(kViewName2, &views::TabbedPaneTab::GetTitleText,
                        kTab3Title));
}

TEST_F(InteractiveViewsTestTest, IfViewTrue) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(const LabelButton*)>,
                         condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step1);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step2);

  EXPECT_CALL(condition, Run(button1_.get())).WillOnce(testing::Return(true));
  EXPECT_CALL(step1, Run);
  RunTestSequence(
      IfView(kButton1Id, condition.Get(), Do(step1.Get()), Do(step2.Get())));
}

TEST_F(InteractiveViewsTestTest, IfViewFalse) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<bool(const LabelButton*)>,
                         condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step1);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step2);

  EXPECT_CALL(condition, Run(button1_.get())).WillOnce(testing::Return(false));
  EXPECT_CALL(step2, Run);
  RunTestSequence(
      IfView(kButton1Id, condition.Get(), Do(step1.Get()), Do(step2.Get())));
}

TEST_F(InteractiveViewsTestTest, IfViewMatchesTrue) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<int(const LabelButton*)>,
                         condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step1);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step2);

  EXPECT_CALL(condition, Run(button1_.get())).WillOnce(testing::Return(1));
  EXPECT_CALL(step1, Run);
  RunTestSequence(IfViewMatches(kButton1Id, condition.Get(), 1, Do(step1.Get()),
                                Do(step2.Get())));
}

TEST_F(InteractiveViewsTestTest, IfViewMatchesFalse) {
  UNCALLED_MOCK_CALLBACK(base::OnceCallback<int(const LabelButton*)>,
                         condition);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step1);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step2);

  EXPECT_CALL(condition, Run(button1_.get())).WillOnce(testing::Return(2));
  EXPECT_CALL(step2, Run);
  RunTestSequence(IfViewMatches(kButton1Id, condition.Get(), 1, Do(step1.Get()),
                                Do(step2.Get())));
}

TEST_F(InteractiveViewsTestTest, IfViewPropertyMatchesTrue) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step1);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step2);

  EXPECT_CALL(step1, Run);
  RunTestSequence(IfViewPropertyMatches(kButton1Id, &LabelButton::GetText,
                                        std::u16string(kButton1Caption),
                                        Do(step1.Get()), Do(step2.Get())));
}

TEST_F(InteractiveViewsTestTest, IfViewPropertyMatchesFalse) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step1);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, step2);

  EXPECT_CALL(step2, Run);
  RunTestSequence(IfViewPropertyMatches(kButton1Id, &LabelButton::GetText,
                                        testing::Ne(kButton1Caption),
                                        Do(step1.Get()), Do(step2.Get())));
}

// Test that elements named in the main test sequence are available in
// subsequences.
TEST_F(InteractiveViewsTestTest, InParallelNamedView) {
  auto is_view = []() {
    return base::BindOnce([](View* actual) { return actual; });
  };

  RunTestSequence(
      // Name two views. Each will be referenced in a subsequence.
      NameView(kViewName, button1_.get()), NameView(kViewName2, button2_.get()),
      // Run subsequences, each of which references a different named view from
      // the outer sequence. Both should succeed.
      InParallel(CheckView(kViewName, is_view(), button1_),
                 CheckView(kViewName2, is_view(), button2_)));
}

// Test that various automatic binding methods work with verbs and conditions.
TEST_F(InteractiveViewsTestTest, BindingMethods) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, correct);
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, incorrect);

  auto get_second_tab = [](TabbedPane* tabs) { return tabs->GetTabAt(1U); };

  EXPECT_CALL(correct, Run).Times(2);
  RunTestSequence(
      SelectTab(kTabbedPaneId, 1U),
      NameViewRelative(kTabbedPaneId, kViewName, get_second_tab),
      WithView(kViewName, [](TabbedPaneTab* tab) { EXPECT_NE(nullptr, tab); }),
      IfView(
          kViewName, [](const TabbedPaneTab* tab) { return tab != nullptr; },
          Do(correct.Get()), Do(incorrect.Get())),
      IfViewMatches(
          kViewName,
          [this](const TabbedPaneTab* tab) { return tabs_->GetIndexOf(tab); },
          0U, Do(incorrect.Get()), Do(correct.Get())));
}

TEST_F(InteractiveViewsTestTest, ScrollIntoView) {
  const auto visible = [this](View* view) {
    const gfx::Rect bounds = view->GetBoundsInScreen();
    const gfx::Rect scroll_bounds = scroll_->GetBoundsInScreen();
    return bounds.Intersects(scroll_bounds);
  };

  RunTestSequence(CheckView(kScrollChild1Id, visible, true),
                  CheckView(kScrollChild2Id, visible, false),
                  ScrollIntoView(kScrollChild2Id),
                  CheckView(kScrollChild2Id, visible, true),
                  ScrollIntoView(kScrollChild1Id),
                  CheckView(kScrollChild1Id, visible, true));
}

}  // namespace views::test

// Verifies that WaitForViewProperty() compiles outside of the views namespace
// (this was a problem previously).
class InteractiveViewsTestCompileTest
    : public views::test::InteractiveViewsTestTest {
 public:
  InteractiveViewsTestCompileTest() = default;
  ~InteractiveViewsTestCompileTest() override = default;

  void WaitForViewPropertyCompileOutsideViews() {
    (void)WaitForViewProperty(views::test::kButton1Id, views::View, Enabled,
                              true);
  }
};

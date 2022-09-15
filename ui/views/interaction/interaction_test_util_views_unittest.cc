// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_test_util_views.h"

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/interaction/interaction_test_util_mac.h"
#endif

namespace views::test {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMenuItemIdentifier);
const char16_t kMenuItem1[] = u"Menu item";
const char16_t kMenuItem2[] = u"Menu item 2";
constexpr int kMenuID1 = 1;
constexpr int kMenuID2 = 2;
}  // namespace

class InteractionTestUtilViewsTest : public ViewsTestBase {
 public:
  InteractionTestUtilViewsTest() = default;
  ~InteractionTestUtilViewsTest() override = default;

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
    WidgetVisibleWaiter visible_waiter(widget.get());
    widget->Show();
    visible_waiter.Wait();
    return widget;
  }

  static View* ElementToView(ui::TrackedElement* element) {
    return element ? element->AsA<TrackedElementViews>()->view() : nullptr;
  }

  void CreateMenuModel() {
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);
    menu_model_->AddItem(kMenuID1, kMenuItem1);
    menu_model_->AddItem(kMenuID2, kMenuItem2);
    menu_model_->SetElementIdentifierAt(1, kMenuItemIdentifier);
  }

  void ShowMenu() {
    CreateMenuModel();

    menu_runner_ =
        std::make_unique<MenuRunner>(menu_model_.get(), MenuRunner::NO_FLAGS);
    menu_runner_->RunMenuAt(
        widget_.get(), nullptr, gfx::Rect(gfx::Point(), gfx::Size(200, 200)),
        MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE);

    menu_item_ = AsViewClass<MenuItemView>(ElementToView(
        ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
            kMenuItemIdentifier,
            ElementTrackerViews::GetContextForView(contents_))));
    Widget* const menu_widget = menu_item_->GetWidget();
    test::WidgetVisibleWaiter visible_waiter(menu_widget);
    visible_waiter.Wait();
    EXPECT_TRUE(menu_item_->GetVisible());
    EXPECT_TRUE(menu_item_->GetWidget()->IsVisible());
  }

  void CloseMenu() {
    menu_runner_.reset();
    menu_model_.reset();
    menu_item_ = nullptr;
  }

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateWidget();
    contents_ = widget_->GetContentsView();
    test_util_ = std::make_unique<ui::test::InteractionTestUtil>();
    test_util_->AddSimulator(
        std::make_unique<InteractionTestUtilSimulatorViews>());
#if BUILDFLAG(IS_MAC)
    test_util_->AddSimulator(
        std::make_unique<ui::test::InteractionTestUtilSimulatorMac>());
#endif
  }

  void TearDown() override {
    test_util_.reset();
    if (menu_runner_)
      CloseMenu();
    widget_.reset();
    contents_ = nullptr;
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<ui::test::InteractionTestUtil> test_util_;
  std::unique_ptr<Widget> widget_;
  raw_ptr<View> contents_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<MenuRunner> menu_runner_;
  raw_ptr<MenuItemView> menu_item_ = nullptr;
};

TEST_F(InteractionTestUtilViewsTest, PressButton_DontCare) {
  UNCALLED_MOCK_CALLBACK(Button::PressedCallback::Callback, pressed);
  auto* const button = contents_->AddChildView(std::make_unique<LabelButton>(
      Button::PressedCallback(pressed.Get()), u"Button"));
  widget_->LayoutRootViewIfNecessary();
  EXPECT_CALL_IN_SCOPE(
      pressed, Run,
      test_util_->PressButton(
          views::ElementTrackerViews::GetInstance()->GetElementForView(button,
                                                                       true)));
}

TEST_F(InteractionTestUtilViewsTest, PressButton_Keyboard) {
  UNCALLED_MOCK_CALLBACK(Button::PressedCallback::Callback, pressed);
  auto* const button = contents_->AddChildView(std::make_unique<LabelButton>(
      Button::PressedCallback(pressed.Get()), u"Button"));
  widget_->LayoutRootViewIfNecessary();
  EXPECT_CALL_IN_SCOPE(
      pressed, Run,
      test_util_->PressButton(
          views::ElementTrackerViews::GetInstance()->GetElementForView(button,
                                                                       true),
          ui::test::InteractionTestUtil::InputType::kKeyboard));
}

TEST_F(InteractionTestUtilViewsTest, PressButton_Mouse) {
  UNCALLED_MOCK_CALLBACK(Button::PressedCallback::Callback, pressed);
  // Add a spacer view to make sure we're actually trying to send events in the
  // appropriate coordinate space.
  contents_->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(), u"Spacer"));
  auto* const button = contents_->AddChildView(std::make_unique<LabelButton>(
      Button::PressedCallback(pressed.Get()), u"Button"));
  widget_->LayoutRootViewIfNecessary();
  EXPECT_CALL_IN_SCOPE(
      pressed, Run,
      test_util_->PressButton(
          views::ElementTrackerViews::GetInstance()->GetElementForView(button,
                                                                       true),
          ui::test::InteractionTestUtil::InputType::kMouse));
}

TEST_F(InteractionTestUtilViewsTest, PressButton_Touch) {
  UNCALLED_MOCK_CALLBACK(Button::PressedCallback::Callback, pressed);
  auto* const button = contents_->AddChildView(std::make_unique<LabelButton>(
      Button::PressedCallback(pressed.Get()), u"Button"));
  widget_->LayoutRootViewIfNecessary();
  EXPECT_CALL_IN_SCOPE(
      pressed, Run,
      test_util_->PressButton(
          views::ElementTrackerViews::GetInstance()->GetElementForView(button,
                                                                       true),
          ui::test::InteractionTestUtil::InputType::kTouch));
}

TEST_F(InteractionTestUtilViewsTest, SelectMenuItem_DontCare) {
  ShowMenu();
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, pressed);
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          kMenuItemIdentifier,
          ElementTrackerViews::GetContextForWidget(widget_.get()),
          pressed.Get());
  EXPECT_CALL_IN_SCOPE(
      pressed, Run,
      test_util_->SelectMenuItem(
          views::ElementTrackerViews::GetInstance()->GetElementForView(
              menu_item_)));
}

TEST_F(InteractionTestUtilViewsTest, SelectMenuItem_Keyboard) {
  ShowMenu();
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, pressed);
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          kMenuItemIdentifier,
          ElementTrackerViews::GetContextForWidget(widget_.get()),
          pressed.Get());
  EXPECT_CALL_IN_SCOPE(
      pressed, Run,
      test_util_->SelectMenuItem(
          views::ElementTrackerViews::GetInstance()->GetElementForView(
              menu_item_),
          ui::test::InteractionTestUtil::InputType::kKeyboard));
}

TEST_F(InteractionTestUtilViewsTest, SelectMenuItem_Mouse) {
  ShowMenu();
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, pressed);
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          kMenuItemIdentifier,
          ElementTrackerViews::GetContextForWidget(widget_.get()),
          pressed.Get());
  EXPECT_CALL_IN_SCOPE(
      pressed, Run,
      test_util_->SelectMenuItem(
          views::ElementTrackerViews::GetInstance()->GetElementForView(
              menu_item_),
          ui::test::InteractionTestUtil::InputType::kMouse));
}

TEST_F(InteractionTestUtilViewsTest, SelectMenuItem_Touch) {
  ShowMenu();
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, pressed);
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          kMenuItemIdentifier,
          ElementTrackerViews::GetContextForWidget(widget_.get()),
          pressed.Get());
  EXPECT_CALL_IN_SCOPE(
      pressed, Run,
      test_util_->SelectMenuItem(
          views::ElementTrackerViews::GetInstance()->GetElementForView(
              menu_item_),
          ui::test::InteractionTestUtil::InputType::kTouch));
}

}  // namespace views::test

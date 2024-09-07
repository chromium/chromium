// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_test_util_views.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/range/range.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/editable_combobox/editable_combobox.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/textfield/textfield.h"
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
const char16_t kTab1Title[] = u"Tab1";
const char16_t kTab2Title[] = u"Tab2";
const char16_t kTab3Title[] = u"Tab3";
const char16_t kComboBoxItem1[] = u"Item1";
const char16_t kComboBoxItem2[] = u"Item2";
const char16_t kComboBoxItem3[] = u"Item3";
constexpr int kMenuID1 = 1;
constexpr int kMenuID2 = 2;

class DefaultActionTestView : public View {
  METADATA_HEADER(DefaultActionTestView, View)

 public:
  DefaultActionTestView() = default;
  ~DefaultActionTestView() override = default;

  bool HandleAccessibleAction(const ui::AXActionData& action_data) override {
    EXPECT_EQ(ax::mojom::Action::kDoDefault, action_data.action);
    EXPECT_FALSE(activated_);
    activated_ = true;
    return true;
  }

  bool activated() const { return activated_; }

 private:
  bool activated_ = false;
};

BEGIN_METADATA(DefaultActionTestView)
END_METADATA

class AcceleratorView : public View {
  METADATA_HEADER(AcceleratorView, View)

 public:
  explicit AcceleratorView(ui::Accelerator accelerator)
      : accelerator_(accelerator) {
    AddAccelerator(accelerator);
  }

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    EXPECT_EQ(accelerator_, accelerator);
    EXPECT_FALSE(pressed_);
    pressed_ = true;
    return true;
  }

  bool CanHandleAccelerators() const override { return true; }

  bool pressed() const { return pressed_; }

 private:
  const ui::Accelerator accelerator_;
  bool pressed_ = false;
};

BEGIN_METADATA(AcceleratorView)
END_METADATA

}  // namespace

class InteractionTestUtilViewsTest
    : public ViewsTestBase,
      public testing::WithParamInterface<
          ui::test::InteractionTestUtil::InputType> {
 public:
  InteractionTestUtilViewsTest() = default;
  ~InteractionTestUtilViewsTest() override = default;

  std::unique_ptr<Widget> CreateWidget() {
    auto widget = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 300, 300);
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
    menu_item_ = nullptr;
    menu_runner_.reset();
    menu_model_.reset();
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
    contents_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<ui::ComboboxModel> CreateComboboxModel() {
    return std::make_unique<ui::SimpleComboboxModel>(
        std::vector<ui::SimpleComboboxModel::Item>{
            ui::SimpleComboboxModel::Item(kComboBoxItem1),
            ui::SimpleComboboxModel::Item(kComboBoxItem2),
            ui::SimpleComboboxModel::Item(kComboBoxItem3)});
  }

 protected:
  std::unique_ptr<ui::test::InteractionTestUtil> test_util_;
  std::unique_ptr<Widget> widget_;
  raw_ptr<View> contents_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<MenuRunner> menu_runner_;
  raw_ptr<MenuItemView> menu_item_ = nullptr;
};

TEST_P(InteractionTestUtilViewsTest, PressButton) {
  UNCALLED_MOCK_CALLBACK(Button::PressedCallback::Callback, pressed);
  // Add a spacer view to make sure we're actually trying to send events in the
  // appropriate coordinate space.
  contents_->AddChildView(
      std::make_unique<LabelButton>(Button::PressedCallback(), u"Spacer"));
  auto* const button = contents_->AddChildView(std::make_unique<LabelButton>(
      Button::PressedCallback(pressed.Get()), u"Button"));
  widget_->LayoutRootViewIfNecessary();
  EXPECT_CALL_IN_SCOPE(pressed, Run,
                       EXPECT_EQ(ui::test::ActionResult::kSucceeded,
                                 test_util_->PressButton(
                                     views::ElementTrackerViews::GetInstance()
                                         ->GetElementForView(button, true),
                                     GetParam())));
}

TEST_P(InteractionTestUtilViewsTest, SelectMenuItem) {
  ShowMenu();
  UNCALLED_MOCK_CALLBACK(ui::ElementTracker::Callback, pressed);
  auto subscription =
      ui::ElementTracker::GetElementTracker()->AddElementActivatedCallback(
          kMenuItemIdentifier,
          ElementTrackerViews::GetContextForWidget(widget_.get()),
          pressed.Get());
  EXPECT_CALL_IN_SCOPE(pressed, Run,
                       EXPECT_EQ(ui::test::ActionResult::kSucceeded,
                                 test_util_->SelectMenuItem(
                                     views::ElementTrackerViews::GetInstance()
                                         ->GetElementForView(menu_item_),
                                     GetParam())));
}

TEST_P(InteractionTestUtilViewsTest, DoDefault) {
  if (GetParam() == ui::test::InteractionTestUtil::InputType::kDontCare) {
    // Unfortunately, buttons don't respond to AX events the same way, so use a
    // custom view for this one case.
    auto* const view =
        contents_->AddChildView(std::make_unique<DefaultActionTestView>());
    widget_->LayoutRootViewIfNecessary();
    EXPECT_EQ(ui::test::ActionResult::kSucceeded,
              test_util_->DoDefaultAction(
                  views::ElementTrackerViews::GetInstance()->GetElementForView(
                      view, true)));
    EXPECT_TRUE(view->activated());

  } else {
    // A button can be used for this because we are simulating a usual event
    // type, which buttons respond to.
    UNCALLED_MOCK_CALLBACK(Button::PressedCallback::Callback, pressed);
    // Add a spacer view to make sure we're actually trying to send events in
    // the appropriate coordinate space.
    contents_->AddChildView(
        std::make_unique<LabelButton>(Button::PressedCallback(), u"Spacer"));
    auto* const button = contents_->AddChildView(std::make_unique<LabelButton>(
        Button::PressedCallback(pressed.Get()), u"Button"));
    widget_->LayoutRootViewIfNecessary();
    EXPECT_CALL_IN_SCOPE(pressed, Run,
                         EXPECT_EQ(ui::test::ActionResult::kSucceeded,
                                   test_util_->DoDefaultAction(
                                       views::ElementTrackerViews::GetInstance()
                                           ->GetElementForView(button, true),
                                       GetParam())));
  }
}

TEST_P(InteractionTestUtilViewsTest, SelectTab) {
  auto* const pane = contents_->AddChildView(std::make_unique<TabbedPane>());
  pane->AddTab(kTab1Title, std::make_unique<LabelButton>(
                               Button::PressedCallback(), u"Button"));
  pane->AddTab(kTab2Title, std::make_unique<LabelButton>(
                               Button::PressedCallback(), u"Button"));
  pane->AddTab(kTab3Title, std::make_unique<LabelButton>(
                               Button::PressedCallback(), u"Button"));
  auto* const pane_el =
      views::ElementTrackerViews::GetInstance()->GetElementForView(pane, true);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectTab(pane_el, 2, GetParam()));
  EXPECT_EQ(2U, pane->GetSelectedTabIndex());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectTab(pane_el, 0, GetParam()));
  EXPECT_EQ(0U, pane->GetSelectedTabIndex());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectTab(pane_el, 1, GetParam()));
  EXPECT_EQ(1U, pane->GetSelectedTabIndex());
}

TEST_P(InteractionTestUtilViewsTest, SelectDropdownItem_Combobox) {
#if BUILDFLAG(IS_MAC)
  // Only kDontCare is supported on Mac.
  if (GetParam() != ui::test::InteractionTestUtil::InputType::kDontCare)
    GTEST_SKIP();
#endif

  auto* const box = contents_->AddChildView(
      std::make_unique<Combobox>(CreateComboboxModel()));
  box->GetViewAccessibility().SetName(u"Combobox");
  widget_->LayoutRootViewIfNecessary();
  auto* const box_el =
      views::ElementTrackerViews::GetInstance()->GetElementForView(box, true);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 2, GetParam()));
  EXPECT_EQ(2U, box->GetSelectedIndex());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 0, GetParam()));
  EXPECT_EQ(0U, box->GetSelectedIndex());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 1, GetParam()));
  EXPECT_EQ(1U, box->GetSelectedIndex());
}

TEST_P(InteractionTestUtilViewsTest, SelectDropdownItem_EditableCombobox) {
#if BUILDFLAG(IS_MAC)
  // Only kDontCare is supported on Mac.
  if (GetParam() != ui::test::InteractionTestUtil::InputType::kDontCare)
    GTEST_SKIP();
#endif

  auto* const box = contents_->AddChildView(
      std::make_unique<EditableCombobox>(CreateComboboxModel()));
  box->GetViewAccessibility().SetName(u"Editable Combobox");
  widget_->LayoutRootViewIfNecessary();
  auto* const box_el =
      views::ElementTrackerViews::GetInstance()->GetElementForView(box, true);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 2, GetParam()));
  EXPECT_EQ(kComboBoxItem3, box->GetText());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 0, GetParam()));
  EXPECT_EQ(kComboBoxItem1, box->GetText());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 1, GetParam()));
  EXPECT_EQ(kComboBoxItem2, box->GetText());
}

TEST_P(InteractionTestUtilViewsTest, SelectDropdownItem_Combobox_NoArrow) {
#if BUILDFLAG(IS_MAC)
  // Only kDontCare is supported on Mac.
  if (GetParam() != ui::test::InteractionTestUtil::InputType::kDontCare)
    GTEST_SKIP();
#endif

  auto* const box = contents_->AddChildView(
      std::make_unique<Combobox>(CreateComboboxModel()));
  box->SetShouldShowArrow(false);
  box->GetViewAccessibility().SetName(u"Combobox");
  widget_->LayoutRootViewIfNecessary();
  auto* const box_el =
      views::ElementTrackerViews::GetInstance()->GetElementForView(box, true);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 2, GetParam()));
  EXPECT_EQ(2U, box->GetSelectedIndex());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 0, GetParam()));
  EXPECT_EQ(0U, box->GetSelectedIndex());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 1, GetParam()));
  EXPECT_EQ(1U, box->GetSelectedIndex());
}

TEST_P(InteractionTestUtilViewsTest,
       SelectDropdownItem_EditableCombobox_NoArrow) {
#if BUILDFLAG(IS_MAC)
  // Only kDontCare is supported on Mac.
  if (GetParam() != ui::test::InteractionTestUtil::InputType::kDontCare)
    GTEST_SKIP();
#endif

  // These cases are not supported for editable combobox without an arrow
  // button; editable comboboxes without arrows trigger on specific text input.
  if (GetParam() == ui::test::InteractionTestUtil::InputType::kMouse ||
      GetParam() == ui::test::InteractionTestUtil::InputType::kTouch) {
    GTEST_SKIP();
  }
  // Pass the default values for every parameter except for `display_arrow`.
  auto* const box = contents_->AddChildView(std::make_unique<EditableCombobox>(
      CreateComboboxModel(), false, true, EditableCombobox::kDefaultTextContext,
      EditableCombobox::kDefaultTextStyle, /* display_arrow =*/false));
  box->GetViewAccessibility().SetName(u"Editable Combobox");
  auto* const box_el =
      views::ElementTrackerViews::GetInstance()->GetElementForView(box, true);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 2, GetParam()));
  EXPECT_EQ(kComboBoxItem3, box->GetText());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 0, GetParam()));
  EXPECT_EQ(kComboBoxItem1, box->GetText());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SelectDropdownItem(box_el, 1, GetParam()));
  EXPECT_EQ(kComboBoxItem2, box->GetText());
}

TEST_F(InteractionTestUtilViewsTest, EnterText_Textfield) {
  auto* const edit = contents_->AddChildView(std::make_unique<Textfield>());
  edit->SetDefaultWidthInChars(20);
  widget_->LayoutRootViewIfNecessary();

  auto* const edit_el =
      views::ElementTrackerViews::GetInstance()->GetElementForView(edit, true);

  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->EnterText(edit_el, u"abcd"));
  EXPECT_EQ(u"abcd", edit->GetText());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->EnterText(
                edit_el, u"efgh",
                ui::test::InteractionTestUtil::TextEntryMode::kReplaceAll));
  EXPECT_EQ(u"efgh", edit->GetText());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->EnterText(
                edit_el, u"abcd",
                ui::test::InteractionTestUtil::TextEntryMode::kAppend));
  EXPECT_EQ(u"efghabcd", edit->GetText());
  edit->SetSelectedRange(gfx::Range(2, 6));
  EXPECT_EQ(
      ui::test::ActionResult::kSucceeded,
      test_util_->EnterText(
          edit_el, u"1234",
          ui::test::InteractionTestUtil::TextEntryMode::kInsertOrReplace));
  EXPECT_EQ(u"ef1234cd", edit->GetText());
}

TEST_F(InteractionTestUtilViewsTest, EnterText_EditableCombobox) {
  auto* const box = contents_->AddChildView(
      std::make_unique<EditableCombobox>(CreateComboboxModel()));
  box->GetViewAccessibility().SetName(u"Editable Combobox");
  widget_->LayoutRootViewIfNecessary();

  auto* const box_el =
      views::ElementTrackerViews::GetInstance()->GetElementForView(box, true);

  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->EnterText(box_el, u"abcd"));
  EXPECT_EQ(u"abcd", box->GetText());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->EnterText(
                box_el, u"efgh",
                ui::test::InteractionTestUtil::TextEntryMode::kReplaceAll));
  EXPECT_EQ(u"efgh", box->GetText());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->EnterText(
                box_el, u"abcd",
                ui::test::InteractionTestUtil::TextEntryMode::kAppend));
  EXPECT_EQ(u"efghabcd", box->GetText());
  box->SelectRange(gfx::Range(2, 6));
  EXPECT_EQ(
      ui::test::ActionResult::kSucceeded,
      test_util_->EnterText(
          box_el, u"1234",
          ui::test::InteractionTestUtil::TextEntryMode::kInsertOrReplace));
  EXPECT_EQ(u"ef1234cd", box->GetText());
}

TEST_F(InteractionTestUtilViewsTest, ActivateSurface) {
  // Create a bubble that will close on deactivation.
  auto dialog_ptr = std::make_unique<BubbleDialogDelegateView>(
      contents_, BubbleBorder::Arrow::TOP_LEFT);
  dialog_ptr->set_close_on_deactivate(true);
  auto* widget = BubbleDialogDelegateView::CreateBubble(std::move(dialog_ptr));
  WidgetVisibleWaiter shown_waiter(widget);
  widget->Show();
  shown_waiter.Wait();

  // Activating the primary widget should close the bubble again.
  WidgetDestroyedWaiter closed_waiter(widget);
  auto* const view_el =
      ElementTrackerViews::GetInstance()->GetElementForView(contents_, true);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->ActivateSurface(view_el));
  closed_waiter.Wait();
}

TEST_F(InteractionTestUtilViewsTest, SendAccelerator) {
  ui::Accelerator accel(ui::VKEY_F5, ui::EF_SHIFT_DOWN);
  ui::Accelerator accel2(ui::VKEY_F6, ui::EF_NONE);
  auto* const view =
      contents_->AddChildView(std::make_unique<AcceleratorView>(accel));
  auto* const view_el =
      ElementTrackerViews::GetInstance()->GetElementForView(view, true);
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SendAccelerator(view_el, accel2));
  EXPECT_FALSE(view->pressed());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util_->SendAccelerator(view_el, accel));
  EXPECT_TRUE(view->pressed());
}

TEST_F(InteractionTestUtilViewsTest, Confirm) {
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, accept);

  auto dialog_ptr = std::make_unique<BubbleDialogDelegateView>(
      contents_, BubbleBorder::Arrow::TOP_LEFT);
  auto* dialog = dialog_ptr.get();
  dialog->SetAcceptCallback(accept.Get());
  auto* widget = BubbleDialogDelegateView::CreateBubble(std::move(dialog_ptr));
  WidgetVisibleWaiter shown_waiter(widget);
  widget->Show();
  shown_waiter.Wait();

  auto* const dialog_el =
      views::ElementTrackerViews::GetInstance()->GetElementForView(dialog,
                                                                   true);

  EXPECT_CALL_IN_SCOPE(accept, Run, {
    EXPECT_EQ(ui::test::ActionResult::kSucceeded,
              test_util_->Confirm(dialog_el));
    WidgetDestroyedWaiter closed_waiter(widget);
    closed_waiter.Wait();
  });
}

INSTANTIATE_TEST_SUITE_P(
    ,
    InteractionTestUtilViewsTest,
    ::testing::Values(ui::test::InteractionTestUtil::InputType::kDontCare,
                      ui::test::InteractionTestUtil::InputType::kMouse,
                      ui::test::InteractionTestUtil::InputType::kKeyboard,
                      ui::test::InteractionTestUtil::InputType::kTouch),
    [](testing::TestParamInfo<ui::test::InteractionTestUtil::InputType>
           input_type) -> std::string {
      switch (input_type.param) {
        case ui::test::InteractionTestUtil::InputType::kDontCare:
          return "DontCare";
        case ui::test::InteractionTestUtil::InputType::kMouse:
          return "Mouse";
        case ui::test::InteractionTestUtil::InputType::kKeyboard:
          return "Keyboard";
        case ui::test::InteractionTestUtil::InputType::kTouch:
          return "Touch";
      }
    });

}  // namespace views::test

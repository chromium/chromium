// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_dialog_model_host.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"

namespace views {

using BubbleDialogModelHostTest = ViewsTestBase;

// TODO(pbos): Consider moving tests from this file into a test base for
// DialogModel that can be instantiated by any DialogModelHost implementation to
// check its compliance.

namespace {
// WeakPtrs to this delegate is used to infer when DialogModel is destroyed.
class WeakDialogModelDelegate : public ui::DialogModelDelegate {
 public:
  base::WeakPtr<WeakDialogModelDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<WeakDialogModelDelegate> weak_ptr_factory_{this};
};

}  // namespace

TEST_F(BubbleDialogModelHostTest, CloseIsSynchronousAndCallsWindowClosing) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);

  auto delegate = std::make_unique<WeakDialogModelDelegate>();
  auto weak_delegate = delegate->GetWeakPtr();

  int window_closing_count = 0;
  auto host = std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder(std::move(delegate))
          .SetDialogDestroyingCallback(base::BindOnce(base::BindOnce(
              [](int* window_closing_count) { ++(*window_closing_count); },
              &window_closing_count)))
          .Build(),
      anchor_widget->GetContentsView(), BubbleBorder::Arrow::TOP_RIGHT);
  auto* host_ptr = host.get();

  Widget* bubble_widget = BubbleDialogDelegate::CreateBubble(std::move(host));
  test::WidgetDestroyedWaiter waiter(bubble_widget);

  EXPECT_EQ(0, window_closing_count);
  DCHECK_EQ(host_ptr, weak_delegate->dialog_model()->host());
  weak_delegate->dialog_model()->host()->Close();
  EXPECT_EQ(1, window_closing_count);

  // The model (and hence delegate) should destroy synchronously, so the
  // WeakPtr should disappear before waiting for the views Widget to close.
  EXPECT_FALSE(weak_delegate);

  waiter.Wait();
}

TEST_F(BubbleDialogModelHostTest, ElementIDsReportedCorrectly) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMenuItemId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOkButtonId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kExtraButtonId);
  constexpr char16_t kMenuItemText[] = u"Menu Item";
  constexpr char16_t kOkButtonText[] = u"OK";
  constexpr char16_t kExtraButtonText[] = u"Button";

  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  anchor_widget->Show();
  const auto context =
      views::ElementTrackerViews::GetContextForWidget(anchor_widget.get());

  ui::DialogModelMenuItem::Params menu_item_params;
  menu_item_params.SetId(kMenuItemId);
  // TODO(crbug.com/40224983): Remove after addressing this issue.
  menu_item_params.SetIsEnabled(false);
  ui::DialogModel::Button::Params ok_button_params;
  ok_button_params.SetId(kOkButtonId);
  ok_button_params.SetLabel(kOkButtonText);
  ui::DialogModel::Button::Params extra_button_params;
  extra_button_params.SetId(kExtraButtonId);
  extra_button_params.SetLabel(kExtraButtonText);
  auto host = std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder()
          .AddMenuItem(ui::ImageModel(), kMenuItemText, base::DoNothing(),
                       menu_item_params)
          .AddOkButton(base::DoNothing(), ok_button_params)
          .AddExtraButton(base::DoNothing(), extra_button_params)
          .Build(),
      anchor_widget->GetContentsView(), BubbleBorder::Arrow::TOP_RIGHT);

  Widget* const bubble_widget =
      BubbleDialogDelegate::CreateBubble(std::move(host));
  test::WidgetVisibleWaiter waiter(bubble_widget);
  bubble_widget->Show();
  waiter.Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());
  EXPECT_NE(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kMenuItemId, context));
  EXPECT_NE(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kOkButtonId, context));
  EXPECT_NE(nullptr, ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                         kExtraButtonId, context));
  bubble_widget->CloseNow();
}

TEST_F(BubbleDialogModelHostTest, DefaultButtonWithoutOverride) {
  auto host = std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder().AddCancelButton(base::DoNothing()).Build(),
      /*anchor_view=*/nullptr, BubbleBorder::Arrow::TOP_RIGHT);
  EXPECT_EQ(host->GetDefaultDialogButton(),
            static_cast<int>(ui::mojom::DialogButton::kCancel));
}

TEST_F(BubbleDialogModelHostTest, OverrideDefaultButton) {
  auto host = std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder()
          .AddCancelButton(base::DoNothing())
          .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
          .Build(),
      /*anchor_view=*/nullptr, BubbleBorder::Arrow::TOP_RIGHT);
  EXPECT_EQ(host->GetDefaultDialogButton(),
            static_cast<int>(ui::mojom::DialogButton::kCancel));
}

TEST_F(BubbleDialogModelHostTest, OverrideNoneDefaultButton) {
  auto host = std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder()
          .AddCancelButton(base::DoNothing())
          .OverrideDefaultButton(ui::mojom::DialogButton::kNone)
          .Build(),
      /*anchor_view=*/nullptr, BubbleBorder::Arrow::TOP_RIGHT);
  EXPECT_EQ(host->GetDefaultDialogButton(),
            static_cast<int>(ui::mojom::DialogButton::kNone));
}

TEST_F(BubbleDialogModelHostTest, OverrideDefaultButtonDeathTest) {
  EXPECT_DCHECK_DEATH(std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder()
          .AddCancelButton(base::DoNothing())
          .OverrideDefaultButton(ui::mojom::DialogButton::kOk)
          .Build(),
      /*anchor_view=*/nullptr, BubbleBorder::Arrow::TOP_RIGHT))
      << "Cannot override the default button with a button which does not "
         "exist.";
}

TEST_F(BubbleDialogModelHostTest,
       SetInitiallyFocusedViewOverridesDefaultButtonFocus) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFocusedField);

  auto host = std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder()
          .AddCancelButton(base::DoNothing())
          .OverrideDefaultButton(ui::mojom::DialogButton::kCancel)
          .AddTextfield(kFocusedField, u"label", u"text")
          .SetInitiallyFocusedField(kFocusedField)
          .Build(),
      /*anchor_view=*/nullptr, BubbleBorder::Arrow::TOP_RIGHT);
  EXPECT_EQ(host->GetDefaultDialogButton(),
            static_cast<int>(ui::mojom::DialogButton::kCancel));
  EXPECT_EQ(host->GetInitiallyFocusedView()->GetProperty(kElementIdentifierKey),
            kFocusedField);
}

TEST_F(BubbleDialogModelHostTest, SetCustomInitiallyFocusedView) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCustomFieldId);

  std::unique_ptr<View> container = Builder<View>().Build();
  std::unique_ptr<Textfield> textfield_unique = Builder<Textfield>().Build();
  raw_ptr<View> textfield = textfield_unique.get();
  container->AddChildView(std::move(textfield_unique));

  auto host = std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder()
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(container),
                  views::BubbleDialogModelHost::FieldType::kControl, textfield),
              kCustomFieldId)
          .SetInitiallyFocusedField(kCustomFieldId)
          .Build(),
      /*anchor_view=*/nullptr, BubbleBorder::Arrow::TOP_RIGHT);

  EXPECT_EQ(host->GetInitiallyFocusedView(), textfield);
  textfield = nullptr;
}

TEST_F(BubbleDialogModelHostTest, SetEnabledButtons) {
  constexpr char16_t kExtraButtonText[] = u"Button";

  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  anchor_widget->Show();

  auto host_unique = std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder()
          .AddOkButton(base::DoNothing())
          .AddCancelButton(base::DoNothing(),
                           ui::DialogModel::Button::Params().SetEnabled(false))
          .AddExtraButton(base::DoNothing(), ui::DialogModel::Button::Params()
                                                 .SetLabel(kExtraButtonText)
                                                 .SetEnabled(true))
          .Build(),
      anchor_widget->GetContentsView(), BubbleBorder::Arrow::TOP_RIGHT);

  auto* host = host_unique.get();
  Widget* const bubble_widget =
      BubbleDialogDelegate::CreateBubble(std::move(host_unique));
  test::WidgetVisibleWaiter waiter(bubble_widget);
  bubble_widget->Show();
  waiter.Wait();

  EXPECT_EQ(host->GetOkButton()->GetEnabled(), true);
  EXPECT_EQ(host->GetCancelButton()->GetEnabled(), false);
  EXPECT_EQ(host->GetExtraView()->GetEnabled(), true);

  bubble_widget->CloseNow();
}

TEST_F(BubbleDialogModelHostTest, TestFieldVisibility) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kField);

  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  anchor_widget->Show();
  const ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(anchor_widget.get());

  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder()
          .AddTextfield(kField, u"label", u"text",
                        ui::DialogModelTextfield::Params().SetVisible(false))
          .Build();

  // Get a raw pointer to the model before we move ownership so it can be
  // changed after the host is created.
  ui::DialogModel* model = dialog_model.get();

  auto host = std::make_unique<BubbleDialogModelHost>(
      std::move(dialog_model), anchor_widget->GetContentsView(),
      BubbleBorder::Arrow::TOP_RIGHT);

  Widget* const bubble_widget =
      BubbleDialogDelegate::CreateBubble(std::move(host));
  test::WidgetVisibleWaiter waiter(bubble_widget);
  bubble_widget->Show();
  waiter.Wait();

  ASSERT_TRUE(bubble_widget->IsVisible());

  // Since the view is invisible, the tracker shouldn't know about it.
  // TODO(crbug.com/40272840): It would be nice to have a means of accessing
  // fields
  //                regardless of state.
  EXPECT_EQ(
      views::ElementTrackerViews::GetInstance()->GetUniqueView(kField, context),
      nullptr);

  model->SetVisible(kField, true);

  // Now that the field is visible, we should be able to access it.
  views::View* const text_field =
      views::ElementTrackerViews::GetInstance()->GetUniqueView(kField, context);

  ASSERT_NE(text_field, nullptr);
  EXPECT_TRUE(text_field->GetVisible());

  bubble_widget->CloseNow();
}

TEST_F(BubbleDialogModelHostTest, TestButtonLabelUpdate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kButtonId);

  constexpr char16_t kStartingButtonLabel[] = u"Starting";
  constexpr char16_t kFinalButtonLabel[] = u"Final";

  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  anchor_widget->Show();

  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder()
          .AddOkButton(base::DoNothing(), ui::DialogModel::Button::Params()
                                              .SetLabel(kStartingButtonLabel)
                                              .SetEnabled(true)
                                              .SetId(kButtonId))
          .Build();

  // Get a raw pointer to the model before we move ownership so it can be
  // changed after the host is created.
  ui::DialogModel* model = dialog_model.get();

  auto host_unique = std::make_unique<BubbleDialogModelHost>(
      std::move(dialog_model), anchor_widget->GetContentsView(),
      BubbleBorder::Arrow::TOP_RIGHT);

  auto* host = host_unique.get();
  Widget* const bubble_widget =
      BubbleDialogDelegate::CreateBubble(std::move(host_unique));
  test::WidgetVisibleWaiter waiter(bubble_widget);
  bubble_widget->Show();
  waiter.Wait();

  model->SetButtonLabel(model->GetButtonByUniqueId(kButtonId),
                        kFinalButtonLabel);

  EXPECT_EQ(host->GetOkButton()->GetEnabled(), true);
  EXPECT_EQ(host->GetOkButton()->GetText(), kFinalButtonLabel);

  bubble_widget->CloseNow();
}

TEST_F(BubbleDialogModelHostTest, TestButtonEnableUpdate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOkButtonId);

  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  anchor_widget->Show();

  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder()
          .AddOkButton(
              base::DoNothing(),
              ui::DialogModel::Button::Params().SetEnabled(false).SetId(
                  kOkButtonId))
          .Build();

  ui::DialogModel* const model = dialog_model.get();

  auto host_unique = std::make_unique<BubbleDialogModelHost>(
      std::move(dialog_model), anchor_widget->GetContentsView(),
      BubbleBorder::Arrow::TOP_RIGHT);

  auto* const host = host_unique.get();
  Widget* const bubble_widget =
      BubbleDialogDelegate::CreateBubble(std::move(host_unique));
  test::WidgetVisibleWaiter waiter(bubble_widget);
  bubble_widget->Show();
  waiter.Wait();

  ui::DialogModel::Button* const ok_button =
      model->GetButtonByUniqueId(kOkButtonId);
  EXPECT_FALSE(ok_button->is_enabled());
  EXPECT_FALSE(host->GetOkButton()->GetEnabled());

  model->SetButtonEnabled(ok_button, /*enabled=*/true);

  EXPECT_TRUE(ok_button->is_enabled());
  EXPECT_TRUE(host->GetOkButton()->GetEnabled());
  bubble_widget->CloseNow();
}

TEST_F(BubbleDialogModelHostTest, TestAddButtonsWithCloseCallback) {
  std::unique_ptr<Widget> anchor_widget = CreateTestWidget(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  anchor_widget->Show();

  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder()
          .AddOkButton(
              base::BindRepeating([] { return false; }),
              ui::DialogModel::Button::Params().SetLabel(u"button").SetEnabled(
                  true))
          .AddCancelButton(
              base::BindRepeating([] { return false; }),
              ui::DialogModel::Button::Params().SetLabel(u"button").SetEnabled(
                  true))
          .Build();

  auto host_unique = std::make_unique<BubbleDialogModelHost>(
      std::move(dialog_model), anchor_widget->GetContentsView(),
      BubbleBorder::Arrow::TOP_RIGHT);

  auto* host = host_unique.get();
  Widget* const bubble_widget =
      BubbleDialogDelegate::CreateBubble(std::move(host_unique));
  test::WidgetVisibleWaiter shown_waiter(bubble_widget);
  bubble_widget->Show();
  shown_waiter.Wait();

  EXPECT_FALSE(host->Accept());
  EXPECT_FALSE(host->Cancel());

  EXPECT_FALSE(bubble_widget->IsClosed());

  bubble_widget->CloseNow();
}

}  // namespace views

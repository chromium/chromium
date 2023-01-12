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
#include "ui/views/controls/button/label_button.h"
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
  std::unique_ptr<Widget> anchor_widget =
      CreateTestWidget(Widget::InitParams::TYPE_WINDOW);

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

  std::unique_ptr<Widget> anchor_widget =
      CreateTestWidget(Widget::InitParams::TYPE_WINDOW);
  anchor_widget->Show();
  const auto context =
      views::ElementTrackerViews::GetContextForWidget(anchor_widget.get());

  ui::DialogModelMenuItem::Params menu_item_params;
  menu_item_params.SetId(kMenuItemId);
  // TODO(crbug.com/1324298): Remove after addressing this issue.
  menu_item_params.SetIsEnabled(false);
  ui::DialogModelButton::Params ok_button_params;
  ok_button_params.SetId(kOkButtonId);
  ok_button_params.SetLabel(kOkButtonText);
  ui::DialogModelButton::Params extra_button_params;
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

TEST_F(BubbleDialogModelHostTest, OverrideDefaultButton) {
  auto host = std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder()
          .AddCancelButton(base::OnceClosure())
          .OverrideDefaultButton(ui::DialogButton::DIALOG_BUTTON_CANCEL)
          .Build(),
      /*anchor_view=*/nullptr, BubbleBorder::Arrow::TOP_RIGHT);
  EXPECT_EQ(host->GetDefaultDialogButton(),
            ui::DialogButton::DIALOG_BUTTON_CANCEL);
}

TEST_F(BubbleDialogModelHostTest, OverrideDefaultButtonDeathTest) {
  EXPECT_DCHECK_DEATH(std::make_unique<BubbleDialogModelHost>(
      ui::DialogModel::Builder()
          .AddCancelButton(base::OnceClosure())
          .OverrideDefaultButton(ui::DialogButton::DIALOG_BUTTON_OK)
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
          .AddCancelButton(base::OnceClosure())
          .OverrideDefaultButton(ui::DialogButton::DIALOG_BUTTON_CANCEL)
          .AddTextfield(kFocusedField, u"label", u"text")
          .SetInitiallyFocusedField(kFocusedField)
          .Build(),
      /*anchor_view=*/nullptr, BubbleBorder::Arrow::TOP_RIGHT);
  EXPECT_EQ(host->GetDefaultDialogButton(),
            ui::DialogButton::DIALOG_BUTTON_CANCEL);
  EXPECT_EQ(host->GetInitiallyFocusedView()->GetProperty(kElementIdentifierKey),
            kFocusedField);
}

}  // namespace views

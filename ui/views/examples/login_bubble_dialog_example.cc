// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/login_bubble_dialog_example.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views {
namespace examples {

namespace {

// Adds a label textfield pair to the login dialog's layout.
Textfield* AddFormRow(LoginBubbleDialogView* bubble,
                      GridLayout* layout,
                      const base::string16& label_text) {
  layout->StartRow(0, 0);
  Label* label = layout->AddView(std::make_unique<Label>(label_text));
  Textfield* textfield = layout->AddView(std::make_unique<Textfield>());
  textfield->SetAssociatedLabel(label);
  textfield->set_controller(bubble);
  constexpr int kDefaultTextfieldWidth = 30;
  constexpr int kMinimumTextfieldWidth = 5;
  textfield->SetDefaultWidthInChars(kDefaultTextfieldWidth);
  textfield->SetMinimumWidthInChars(kMinimumTextfieldWidth);
  return textfield;
}

}  // namespace

// static
void LoginBubbleDialogView::Show(View* anchor_view,
                                 BubbleBorder::Arrow anchor_position,
                                 OnSubmitCallback accept_callback) {
  // LoginBubbleDialogView will be destroyed by the widget when the created
  // widget is destroyed.
  BubbleDialogDelegateView::CreateBubble(
      new LoginBubbleDialogView(anchor_view, anchor_position,
                                std::move(accept_callback)))
      ->Show();
}

LoginBubbleDialogView::~LoginBubbleDialogView() = default;

void LoginBubbleDialogView::ContentsChanged(
    Textfield* sender,
    const base::string16& new_contents) {
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, !username_->GetText().empty() &&
                                             !password_->GetText().empty());
  DialogModelChanged();
}

LoginBubbleDialogView::LoginBubbleDialogView(
    View* anchor_view,
    BubbleBorder::Arrow anchor_position,
    OnSubmitCallback accept_callback)
    : BubbleDialogDelegateView(anchor_view, anchor_position) {
  SetButtonEnabled(ui::DIALOG_BUTTON_OK, false);

  const auto on_submit = [](const LoginBubbleDialogView* bubble_view,
                            OnSubmitCallback accept_callback) {
    std::move(accept_callback)
        .Run(bubble_view->username_->GetText(),
             bubble_view->password_->GetText());
  };
  SetAcceptCallback(base::BindOnce(on_submit, base::Unretained(this),
                                   std::move(accept_callback)));

  SetTitle(l10n_util::GetStringUTF16(IDS_LOGIN_TITLE_LABEL));
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_LOGIN_OK_BUTTON_LABEL));

  const LayoutProvider* provider = LayoutProvider::Get();
  set_margins(
      provider->GetDialogInsetsForContentType(views::CONTROL, views::CONTROL));
  const int related_control_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int label_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  GridLayout* layout = SetLayoutManager(std::make_unique<GridLayout>());
  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                        GridLayout::kFixedSize,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(0, label_padding);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);

  username_ = AddFormRow(this, layout,
                         l10n_util::GetStringUTF16(IDS_LOGIN_USERNAME_LABEL));

  layout->AddPaddingRow(0, related_control_padding);

  password_ = AddFormRow(this, layout,
                         l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_LABEL));
  password_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
}

LoginBubbleDialogExample::LoginBubbleDialogExample()
    : ExampleBase(GetStringUTF8(IDS_LOGIN_SELECT_LABEL).c_str()) {}

LoginBubbleDialogExample::~LoginBubbleDialogExample() = default;

void LoginBubbleDialogExample::CreateExampleView(View* container) {
  const int related_control_padding = LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int label_padding = LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<GridLayout>());
  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                        GridLayout::kFixedSize,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(0, label_padding);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1.0,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRowWithPadding(0, 0, 0, related_control_padding);
  button_ = layout->AddView(std::make_unique<MdTextButton>(
      Button::PressedCallback(), GetStringUTF16(IDS_LOGIN_SHOW_BUTTON_LABEL)));
  button_->SetCallback(base::BindRepeating(
      &LoginBubbleDialogView::Show, button_, BubbleBorder::TOP_LEFT,
      base::BindRepeating(&LoginBubbleDialogExample::OnSubmit,
                          base::Unretained(this))));

  layout->StartRowWithPadding(0, 0, 0, related_control_padding);
  username_label_ = layout->AddView(std::make_unique<Label>(
      l10n_util::GetStringUTF16(IDS_LOGIN_USERNAME_LABEL)));
  username_label_->SetVisible(false);
  username_input_ = layout->AddView(std::make_unique<Label>());

  layout->StartRowWithPadding(0, 0, 0, related_control_padding);
  password_label_ = layout->AddView(std::make_unique<Label>(
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_LABEL)));
  password_label_->SetVisible(false);
  password_input_ = layout->AddView(std::make_unique<Label>());
}

void LoginBubbleDialogExample::OnSubmit(base::string16 username,
                                        base::string16 password) {
  username_label_->SetVisible(true);
  username_input_->SetText(username);
  password_label_->SetVisible(true);
  password_input_->SetText(password);
}

}  // namespace examples
}  // namespace views

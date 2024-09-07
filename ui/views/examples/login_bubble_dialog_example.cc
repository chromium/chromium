// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/login_bubble_dialog_example.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views::examples {

namespace {

// Adds a label textfield pair to the login dialog's layout.
Textfield* AddFormRow(LoginBubbleDialogView* bubble,
                      const std::u16string& label_text) {
  Label* label = bubble->AddChildView(std::make_unique<Label>(label_text));
  Textfield* textfield = bubble->AddChildView(std::make_unique<Textfield>());
  textfield->GetViewAccessibility().SetName(*label);
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
    const std::u16string& new_contents) {
  SetButtonEnabled(
      ui::mojom::DialogButton::kOk,
      !username_->GetText().empty() && !password_->GetText().empty());
  DialogModelChanged();
}

LoginBubbleDialogView::LoginBubbleDialogView(
    View* anchor_view,
    BubbleBorder::Arrow anchor_position,
    OnSubmitCallback accept_callback)
    : BubbleDialogDelegateView(anchor_view, anchor_position) {
  SetButtonEnabled(ui::mojom::DialogButton::kOk, false);

  const auto on_submit = [](const LoginBubbleDialogView* bubble_view,
                            OnSubmitCallback accept_callback) {
    std::move(accept_callback)
        .Run(bubble_view->username_->GetText(),
             bubble_view->password_->GetText());
  };
  SetAcceptCallback(base::BindOnce(on_submit, base::Unretained(this),
                                   std::move(accept_callback)));

  SetTitle(l10n_util::GetStringUTF16(IDS_LOGIN_TITLE_LABEL));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_LOGIN_OK_BUTTON_LABEL));

  const LayoutProvider* provider = LayoutProvider::Get();
  set_margins(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));
  const int related_control_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int label_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  SetLayoutManager(std::make_unique<TableLayout>())
      ->AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStretch,
                  TableLayout::kFixedSize,
                  TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(TableLayout::kFixedSize, label_padding)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 1.0,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, TableLayout::kFixedSize)
      .AddPaddingRow(TableLayout::kFixedSize, related_control_padding)
      .AddRows(1, TableLayout::kFixedSize);

  username_ =
      AddFormRow(this, l10n_util::GetStringUTF16(IDS_LOGIN_USERNAME_LABEL));
  password_ =
      AddFormRow(this, l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_LABEL));
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

  container->SetLayoutManager(std::make_unique<TableLayout>())
      ->AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStretch,
                  TableLayout::kFixedSize,
                  TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(TableLayout::kFixedSize, label_padding)
      .AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStretch, 1.0,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingRow(TableLayout::kFixedSize, related_control_padding)
      .AddRows(1, TableLayout::kFixedSize)
      .AddPaddingRow(TableLayout::kFixedSize, related_control_padding)
      .AddRows(1, TableLayout::kFixedSize)
      .AddPaddingRow(TableLayout::kFixedSize, related_control_padding)
      .AddRows(1, TableLayout::kFixedSize);

  button_ = container->AddChildView(std::make_unique<MdTextButton>(
      Button::PressedCallback(), GetStringUTF16(IDS_LOGIN_SHOW_BUTTON_LABEL)));
  button_->SetCallback(base::BindRepeating(
      &LoginBubbleDialogView::Show, button_, BubbleBorder::TOP_LEFT,
      base::BindRepeating(&LoginBubbleDialogExample::OnSubmit,
                          base::Unretained(this))));
  container->AddChildView(std::make_unique<View>());

  username_label_ = container->AddChildView(std::make_unique<Label>(
      l10n_util::GetStringUTF16(IDS_LOGIN_USERNAME_LABEL)));
  username_label_->SetVisible(false);
  username_input_ = container->AddChildView(std::make_unique<Label>());

  password_label_ = container->AddChildView(std::make_unique<Label>(
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_LABEL)));
  password_label_->SetVisible(false);
  password_input_ = container->AddChildView(std::make_unique<Label>());
}

void LoginBubbleDialogExample::OnSubmit(std::u16string username,
                                        std::u16string password) {
  username_label_->SetVisible(true);
  username_input_->SetText(username);
  password_label_->SetVisible(true);
  password_input_->SetText(password);
}

}  // namespace views::examples

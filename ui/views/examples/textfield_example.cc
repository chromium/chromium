// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/textfield_example.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views::examples {

TextfieldExample::TextfieldExample()
    : ExampleBase(GetStringUTF8(IDS_TEXTFIELD_SELECT_LABEL).c_str()) {}

TextfieldExample::~TextfieldExample() {
  if (name_) {
    name_->set_controller(nullptr);
  }
  if (password_) {
    password_->set_controller(nullptr);
  }
}

void TextfieldExample::CreateExampleView(View* container) {
  TableLayout* layout =
      container->SetLayoutManager(std::make_unique<views::TableLayout>());
  layout
      ->AddColumn(LayoutAlignment::kStart, LayoutAlignment::kStretch, 0.2f,
                  TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 0.8f,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0);
  for (int i = 0; i < 12; ++i) {
    layout->AddPaddingRow(TableLayout::kFixedSize, 5)
        .AddRows(1, TableLayout::kFixedSize);
  }

  container->AddChildView(
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_NAME_LABEL)));
  name_ = container->AddChildView(std::make_unique<Textfield>());
  name_->set_controller(this);
  name_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TEXTFIELD_NAME_LABEL));

  container->AddChildView(
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_PASSWORD_LABEL)));
  password_ = container->AddChildView(std::make_unique<Textfield>());
  password_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  password_->SetPlaceholderText(
      GetStringUTF16(IDS_TEXTFIELD_PASSWORD_PLACEHOLDER));
  password_->set_controller(this);
  password_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TEXTFIELD_PASSWORD_LABEL));

  container->AddChildView(
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_DISABLED_LABEL)));
  disabled_ = container->AddChildView(std::make_unique<Textfield>());
  disabled_->SetEnabled(false);
  disabled_->SetText(GetStringUTF16(IDS_TEXTFIELD_DISABLED_PLACEHOLDER));
  disabled_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TEXTFIELD_DISABLED_LABEL));

  container->AddChildView(
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_READ_ONLY_LABEL)));
  read_only_ = container->AddChildView(std::make_unique<Textfield>());
  read_only_->SetReadOnly(true);
  read_only_->SetText(GetStringUTF16(IDS_TEXTFIELD_READ_ONLY_PLACEHOLDER));
  read_only_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TEXTFIELD_READ_ONLY_LABEL));

  container->AddChildView(
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_INVALID_LABEL)));
  invalid_ = container->AddChildView(std::make_unique<Textfield>());
  invalid_->SetInvalid(true);
  invalid_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TEXTFIELD_INVALID_LABEL));
  invalid_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_TEXTFIELD_INVALID_PLACEHOLDER));
  container->AddChildView(
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_RTL_LABEL)));
  rtl_ = container->AddChildView(std::make_unique<Textfield>());
  rtl_->ChangeTextDirectionAndLayoutAlignment(base::i18n::RIGHT_TO_LEFT);
  rtl_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TEXTFIELD_RTL_LABEL));

  show_password_ =
      container->AddChildView(std::make_unique<LabelButton>(
          base::BindRepeating(
              [](TextfieldExample* example) {
                PrintStatus(
                    "Password [%s]",
                    base::UTF16ToUTF8(example->password_->GetText()).c_str());
              },
              base::Unretained(this)),
          GetStringUTF16(IDS_TEXTFIELD_SHOW_PASSWORD_LABEL)));
  container->AddChildView(std::make_unique<View>());
  set_background_ = container->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&Textfield::SetBackgroundColor,
                          base::Unretained(password_), gfx::kGoogleRed300),
      GetStringUTF16(IDS_TEXTFIELD_BACKGROUND_LABEL)));
  container->AddChildView(std::make_unique<View>());
  clear_all_ = container->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&TextfieldExample::ClearAllButtonPressed,
                          base::Unretained(this)),
      GetStringUTF16(IDS_TEXTFIELD_CLEAR_LABEL)));
  container->AddChildView(std::make_unique<View>());
  append_ = container->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&TextfieldExample::AppendButtonPressed,
                          base::Unretained(this)),
      GetStringUTF16(IDS_TEXTFIELD_APPEND_LABEL)));
  container->AddChildView(std::make_unique<View>());
  set_ = container->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&TextfieldExample::SetButtonPressed,
                          base::Unretained(this)),
      GetStringUTF16(IDS_TEXTFIELD_SET_LABEL)));
  container->AddChildView(std::make_unique<View>());
  set_style_ = container->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&TextfieldExample::SetStyleButtonPressed,
                          base::Unretained(this)),
      GetStringUTF16(IDS_TEXTFIELD_SET_STYLE_LABEL)));
}

bool TextfieldExample::HandleKeyEvent(Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  return false;
}

bool TextfieldExample::HandleMouseEvent(Textfield* sender,
                                        const ui::MouseEvent& mouse_event) {
  PrintStatus("HandleMouseEvent click count=%d", mouse_event.GetClickCount());
  return false;
}

void TextfieldExample::ClearAllButtonPressed() {
  name_->SetText(std::u16string());
  password_->SetText(std::u16string());
  disabled_->SetText(std::u16string());
  read_only_->SetText(std::u16string());
  invalid_->SetText(std::u16string());
  rtl_->SetText(std::u16string());
}

void TextfieldExample::AppendButtonPressed() {
  const std::u16string append_text =
      GetStringUTF16(IDS_TEXTFIELD_APPEND_UPDATE_TEXT);
  name_->AppendText(append_text);
  password_->AppendText(append_text);
  disabled_->SetText(append_text);
  read_only_->AppendText(append_text);
  invalid_->AppendText(append_text);
  rtl_->AppendText(append_text);
}

void TextfieldExample::SetButtonPressed() {
  const std::u16string set_text = GetStringUTF16(IDS_TEXTFIELD_SET_UPDATE_TEXT);
  name_->SetText(set_text);
  password_->SetText(set_text);
  disabled_->SetText(set_text);
  read_only_->SetText(set_text);
  invalid_->SetText(set_text);
  rtl_->SetText(set_text);
}

void TextfieldExample::SetStyleButtonPressed() {
  if (name_->GetText().empty())
    return;
  auto* const cp = name_->GetColorProvider();
  name_->SetColor(cp->GetColor(ExamplesColorIds::kColorTextfieldExampleName));

  const size_t fifth = name_->GetText().length() / 5;
  if (!fifth)
    return;

  const gfx::Range big_range(1 * fifth, 4 * fifth);
  name_->ApplyStyle(gfx::TEXT_STYLE_UNDERLINE, true, big_range);
  name_->ApplyColor(
      cp->GetColor(ExamplesColorIds::kColorTextfieldExampleBigRange),
      big_range);

  const gfx::Range small_range(2 * fifth, 3 * fifth);
  name_->ApplyStyle(gfx::TEXT_STYLE_ITALIC, true, small_range);
  name_->ApplyStyle(gfx::TEXT_STYLE_UNDERLINE, false, small_range);
  name_->ApplyColor(
      cp->GetColor(ExamplesColorIds::kColorTextfieldExampleSmallRange),
      small_range);
}

}  // namespace views::examples

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/textfield_example.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views {
namespace examples {

namespace {

template <class K, class T>
T* MakeRow(GridLayout* layout,
           std::unique_ptr<K> view1,
           std::unique_ptr<T> view2) {
  layout->StartRowWithPadding(0, 0, 0, 5);
  if (view1)
    layout->AddView(std::move(view1));
  return layout->AddView(std::move(view2));
}

}  // namespace

TextfieldExample::TextfieldExample()
    : ExampleBase(GetStringUTF8(IDS_TEXTFIELD_SELECT_LABEL).c_str()) {}

TextfieldExample::~TextfieldExample() = default;

void TextfieldExample::CreateExampleView(View* container) {
  auto name = std::make_unique<Textfield>();
  name->set_controller(this);
  auto password = std::make_unique<Textfield>();
  password->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  password->SetPlaceholderText(
      GetStringUTF16(IDS_TEXTFIELD_PASSWORD_PLACEHOLDER));
  password->set_controller(this);
  auto disabled = std::make_unique<Textfield>();
  disabled->SetEnabled(false);
  disabled->SetText(GetStringUTF16(IDS_TEXTFIELD_DISABLED_PLACEHOLDER));
  auto read_only = std::make_unique<Textfield>();
  read_only->SetReadOnly(true);
  read_only->SetText(GetStringUTF16(IDS_TEXTFFIELD_READ_ONLY_PLACEHOLDER));
  auto invalid = std::make_unique<Textfield>();
  invalid->SetInvalid(true);
  auto rtl = std::make_unique<Textfield>();
  rtl->ChangeTextDirectionAndLayoutAlignment(base::i18n::RIGHT_TO_LEFT);

  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0.2f,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0.8f,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);

  name_ = MakeRow(
      layout, std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_NAME_LABEL)),
      std::move(name));
  password_ = MakeRow(
      layout,
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_PASSWORD_LABEL)),
      std::move(password));
  disabled_ = MakeRow(
      layout,
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_DISABLED_LABEL)),
      std::move(disabled));
  read_only_ = MakeRow(
      layout,
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_READ_ONLY_LABEL)),
      std::move(read_only));
  invalid_ = MakeRow(
      layout,
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_INVALID_LABEL)),
      std::move(invalid));
  rtl_ = MakeRow(
      layout, std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_RTL_LABEL)),
      std::move(rtl));
  MakeRow<View, Label>(
      layout, nullptr,
      std::make_unique<Label>(GetStringUTF16(IDS_TEXTFIELD_NAME_LABEL)));
  show_password_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(
          base::BindRepeating(
              [](TextfieldExample* example) {
                PrintStatus(
                    "Password [%s]",
                    base::UTF16ToUTF8(example->password_->GetText()).c_str());
              },
              base::Unretained(this)),
          GetStringUTF16(IDS_TEXTFIELD_SHOW_PASSWORD_LABEL)));
  set_background_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(
          base::BindRepeating(&Textfield::SetBackgroundColor,
                              base::Unretained(password_), gfx::kGoogleRed300),
          GetStringUTF16(IDS_TEXTFIELD_BACKGROUND_LABEL)));
  clear_all_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(
          base::BindRepeating(&TextfieldExample::ClearAllButtonPressed,
                              base::Unretained(this)),
          GetStringUTF16(IDS_TEXTFIELD_CLEAR_LABEL)));
  append_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(
          base::BindRepeating(&TextfieldExample::AppendButtonPressed,
                              base::Unretained(this)),
          GetStringUTF16(IDS_TEXTFIELD_APPEND_LABEL)));
  set_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(
          base::BindRepeating(&TextfieldExample::SetButtonPressed,
                              base::Unretained(this)),
          GetStringUTF16(IDS_TEXTFIELD_SET_LABEL)));
  set_style_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(
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
  name_->SetText(base::string16());
  password_->SetText(base::string16());
  disabled_->SetText(base::string16());
  read_only_->SetText(base::string16());
  invalid_->SetText(base::string16());
  rtl_->SetText(base::string16());
}

void TextfieldExample::AppendButtonPressed() {
  const base::string16 append_text =
      GetStringUTF16(IDS_TEXTFIELD_APPEND_UPDATE_TEXT);
  name_->AppendText(append_text);
  password_->AppendText(append_text);
  disabled_->SetText(append_text);
  read_only_->AppendText(append_text);
  invalid_->AppendText(append_text);
  rtl_->AppendText(append_text);
}

void TextfieldExample::SetButtonPressed() {
  const base::string16 set_text = GetStringUTF16(IDS_TEXTFIELD_SET_UPDATE_TEXT);
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
  name_->SetColor(SK_ColorGREEN);

  const size_t fifth = name_->GetText().length() / 5;
  if (!fifth)
    return;

  const gfx::Range big_range(1 * fifth, 4 * fifth);
  name_->ApplyStyle(gfx::TEXT_STYLE_UNDERLINE, true, big_range);
  name_->ApplyColor(SK_ColorBLUE, big_range);

  const gfx::Range small_range(2 * fifth, 3 * fifth);
  name_->ApplyStyle(gfx::TEXT_STYLE_ITALIC, true, small_range);
  name_->ApplyStyle(gfx::TEXT_STYLE_UNDERLINE, false, small_range);
  name_->ApplyColor(SK_ColorRED, small_range);
}

}  // namespace examples
}  // namespace views

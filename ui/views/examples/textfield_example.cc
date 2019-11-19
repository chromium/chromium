// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/textfield_example.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;
using base::UTF16ToUTF8;

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

TextfieldExample::TextfieldExample() : ExampleBase("Textfield") {}

TextfieldExample::~TextfieldExample() = default;

void TextfieldExample::CreateExampleView(View* container) {
  auto name = std::make_unique<Textfield>();
  name->set_controller(this);
  auto password = std::make_unique<Textfield>();
  password->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  password->SetPlaceholderText(ASCIIToUTF16("password"));
  password->set_controller(this);
  auto disabled = std::make_unique<Textfield>();
  disabled->SetEnabled(false);
  disabled->SetText(ASCIIToUTF16("disabled"));
  auto read_only = std::make_unique<Textfield>();
  read_only->SetReadOnly(true);
  read_only->SetText(ASCIIToUTF16("read only"));
  auto invalid = std::make_unique<Textfield>();
  invalid->SetInvalid(true);
  auto rtl = std::make_unique<Textfield>();
  rtl->ChangeTextDirectionAndLayoutAlignment(base::i18n::RIGHT_TO_LEFT);

  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());

  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL,
                        0.2f, GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                        0.8f, GridLayout::USE_PREF, 0, 0);

  name_ = MakeRow(layout, std::make_unique<Label>(ASCIIToUTF16("Name:")),
                  std::move(name));
  password_ =
      MakeRow(layout, std::make_unique<Label>(ASCIIToUTF16("Password:")),
              std::move(password));
  disabled_ =
      MakeRow(layout, std::make_unique<Label>(ASCIIToUTF16("Disabled:")),
              std::move(disabled));
  read_only_ =
      MakeRow(layout, std::make_unique<Label>(ASCIIToUTF16("Read Only:")),
              std::move(read_only));
  invalid_ = MakeRow(layout, std::make_unique<Label>(ASCIIToUTF16("Invalid:")),
                     std::move(invalid));
  rtl_ = MakeRow(layout, std::make_unique<Label>(ASCIIToUTF16("RTL:")),
                 std::move(rtl));
  MakeRow<View, Label>(layout, nullptr,
                       std::make_unique<Label>(ASCIIToUTF16("Name:")));
  show_password_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Show password")));
  set_background_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(
          this, ASCIIToUTF16("Set non-default background")));
  clear_all_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Clear All")));
  append_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Append")));
  set_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Set")));
  set_style_ = MakeRow<View, LabelButton>(
      layout, nullptr,
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Set Styles")));
}

void TextfieldExample::ContentsChanged(Textfield* sender,
                                       const base::string16& new_contents) {
  if (sender == name_) {
    PrintStatus("Name [%s]", UTF16ToUTF8(new_contents).c_str());
  } else if (sender == password_) {
    PrintStatus("Password [%s]", UTF16ToUTF8(new_contents).c_str());
  } else {
    NOTREACHED();
  }
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

void TextfieldExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == show_password_) {
    PrintStatus("Password [%s]", UTF16ToUTF8(password_->GetText()).c_str());
  } else if (sender == set_background_) {
    password_->SetBackgroundColor(gfx::kGoogleRed300);
  } else if (sender == clear_all_) {
    base::string16 empty;
    name_->SetText(empty);
    password_->SetText(empty);
    disabled_->SetText(empty);
    read_only_->SetText(empty);
    invalid_->SetText(empty);
    rtl_->SetText(empty);
  } else if (sender == append_) {
    name_->AppendText(ASCIIToUTF16("[append]"));
    password_->AppendText(ASCIIToUTF16("[append]"));
    disabled_->SetText(ASCIIToUTF16("[append]"));
    read_only_->AppendText(ASCIIToUTF16("[append]"));
    invalid_->AppendText(ASCIIToUTF16("[append]"));
    rtl_->AppendText(ASCIIToUTF16("[append]"));
  } else if (sender == set_) {
    name_->SetText(ASCIIToUTF16("[set]"));
    password_->SetText(ASCIIToUTF16("[set]"));
    disabled_->SetText(ASCIIToUTF16("[set]"));
    read_only_->SetText(ASCIIToUTF16("[set]"));
    invalid_->SetText(ASCIIToUTF16("[set]"));
    rtl_->SetText(ASCIIToUTF16("[set]"));
  } else if (sender == set_style_) {
    if (!name_->GetText().empty()) {
      name_->SetColor(SK_ColorGREEN);

      if (name_->GetText().length() >= 5) {
        size_t fifth = name_->GetText().length() / 5;
        const gfx::Range big_range(1 * fifth, 4 * fifth);
        name_->ApplyStyle(gfx::TEXT_STYLE_UNDERLINE, true, big_range);
        name_->ApplyColor(SK_ColorBLUE, big_range);

        const gfx::Range small_range(2 * fifth, 3 * fifth);
        name_->ApplyStyle(gfx::TEXT_STYLE_ITALIC, true, small_range);
        name_->ApplyStyle(gfx::TEXT_STYLE_UNDERLINE, false, small_range);
        name_->ApplyColor(SK_ColorRED, small_range);
      }
    }
  }
}

}  // namespace examples
}  // namespace views

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/message_box_view.h"

#include <stddef.h>

#include <memory>
#include <numeric>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr int kDefaultMessageWidth = 400;

// Paragraph separators are defined in
// http://www.unicode.org/Public/6.0.0/ucd/extracted/DerivedBidiClass.txt
//
// # Bidi_Class=Paragraph_Separator
//
// 000A          ; B # Cc       <control-000A>
// 000D          ; B # Cc       <control-000D>
// 001C..001E    ; B # Cc   [3] <control-001C>..<control-001E>
// 0085          ; B # Cc       <control-0085>
// 2029          ; B # Zp       PARAGRAPH SEPARATOR
bool IsParagraphSeparator(base::char16 c) {
  return (c == 0x000A || c == 0x000D || c == 0x001C || c == 0x001D ||
          c == 0x001E || c == 0x0085 || c == 0x2029);
}

// Splits |text| into a vector of paragraphs.
// Given an example "\nabc\ndef\n\n\nhij\n", the split results should be:
// "", "abc", "def", "", "", "hij", and "".
void SplitStringIntoParagraphs(const base::string16& text,
                               std::vector<base::string16>* paragraphs) {
  paragraphs->clear();

  size_t start = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    if (IsParagraphSeparator(text[i])) {
      paragraphs->push_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  paragraphs->push_back(text.substr(start, text.length() - start));
}

}  // namespace

namespace views {

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, public:

MessageBoxView::MessageBoxView(const base::string16& message,
                               bool detect_directionality)
    : inter_row_vertical_spacing_(LayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL)),
      message_width_(kDefaultMessageWidth) {
  const LayoutProvider* provider = LayoutProvider::Get();

  auto message_contents = std::make_unique<View>();
  // We explicitly set insets on the message contents instead of the scroll view
  // so that the scroll view borders are not capped by dialog insets.
  message_contents->SetBorder(CreateEmptyBorder(GetHorizontalInsets(provider)));
  message_contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  auto add_label = [&message_contents, this](
                       const base::string16& text, bool multi_line,
                       gfx::HorizontalAlignment alignment) {
    auto message_label =
        std::make_unique<Label>(text, style::CONTEXT_DIALOG_BODY_TEXT);
    message_label->SetMultiLine(!text.empty());
    message_label->SetAllowCharacterBreak(true);
    message_label->SetHorizontalAlignment(alignment);
    message_labels_.push_back(
        message_contents->AddChildView(std::move(message_label)));
  };
  if (detect_directionality) {
    std::vector<base::string16> texts;
    SplitStringIntoParagraphs(message, &texts);
    for (const auto& text : texts) {
      // Avoid empty multi-line labels, which have a height of 0.
      add_label(text, !text.empty(), gfx::ALIGN_TO_HEAD);
    }
  } else {
    add_label(message, true, gfx::ALIGN_LEFT);
  }
  auto scroll_view = std::make_unique<ScrollView>();
  scroll_view->ClipHeightTo(0, provider->GetDistanceMetric(
                                   DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT));
  scroll_view->SetContents(std::move(message_contents));
  scroll_view_ = AddChildView(std::move(scroll_view));
  // Don't enable text selection if multiple labels are used, since text
  // selection can't span multiple labels.
  if (message_labels_.size() == 1u)
    message_labels_[0]->SetSelectable(true);

  prompt_field_ = AddChildView(std::make_unique<Textfield>());
  prompt_field_->SetAccessibleName(message);
  prompt_field_->SetVisible(false);

  checkbox_ = AddChildView(std::make_unique<Checkbox>());
  checkbox_->SetVisible(false);

  link_ = AddChildView(std::make_unique<Link>());
  link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link_->SetVisible(false);

  ResetLayoutManager();
}

MessageBoxView::~MessageBoxView() = default;

views::Textfield* MessageBoxView::GetVisiblePromptField() {
  return prompt_field_ && prompt_field_->GetVisible() ? prompt_field_ : nullptr;
}

base::string16 MessageBoxView::GetInputText() {
  return prompt_field_ && prompt_field_->GetVisible() ? prompt_field_->GetText()
                                                      : base::string16();
}

bool MessageBoxView::HasVisibleCheckBox() const {
  return checkbox_ && checkbox_->GetVisible();
}

bool MessageBoxView::IsCheckBoxSelected() {
  return checkbox_ && checkbox_->GetVisible() && checkbox_->GetChecked();
}

void MessageBoxView::SetCheckBoxLabel(const base::string16& label) {
  DCHECK(checkbox_);
  if (checkbox_->GetVisible() && checkbox_->GetText() == label)
    return;

  checkbox_->SetText(label);
  checkbox_->SetVisible(true);
  ResetLayoutManager();
}

void MessageBoxView::SetCheckBoxSelected(bool selected) {
  // Only update the checkbox's state after the checkbox is shown.
  if (!checkbox_->GetVisible())
    return;
  checkbox_->SetChecked(selected);
}

void MessageBoxView::SetLink(const base::string16& text,
                             Link::ClickedCallback callback) {
  DCHECK(!text.empty());
  DCHECK(!callback.is_null());
  DCHECK(link_);

  link_->SetCallback(std::move(callback));
  if (link_->GetVisible() && link_->GetText() == text)
    return;
  link_->SetText(text);
  link_->SetVisible(true);
  ResetLayoutManager();
}

void MessageBoxView::SetInterRowVerticalSpacing(int spacing) {
  if (inter_row_vertical_spacing_ == spacing)
    return;

  inter_row_vertical_spacing_ = spacing;
  ResetLayoutManager();
}

void MessageBoxView::SetMessageWidth(int width) {
  if (message_width_ == width)
    return;

  message_width_ = width;
  ResetLayoutManager();
}

void MessageBoxView::SetPromptField(const base::string16& default_prompt) {
  DCHECK(prompt_field_);
  if (prompt_field_->GetVisible() && prompt_field_->GetText() == default_prompt)
    return;
  prompt_field_->SetText(default_prompt);
  prompt_field_->SetVisible(true);
  ResetLayoutManager();
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, View overrides:

void MessageBoxView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.child == this && details.is_add) {
    if (prompt_field_ && prompt_field_->GetVisible())
      prompt_field_->SelectAll(true);
  }
}

bool MessageBoxView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // We only accept Ctrl-C.
  DCHECK(accelerator.key_code() == 'C' && accelerator.IsCtrlDown());

  // We must not intercept Ctrl-C when we have a text box and it's focused.
  if (prompt_field_ && prompt_field_->HasFocus())
    return false;

  // Don't intercept Ctrl-C if we only use a single message label supporting
  // text selection.
  if (message_labels_.size() == 1u && message_labels_[0]->GetSelectable())
    return false;

  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(std::accumulate(message_labels_.cbegin(),
                                message_labels_.cend(), base::string16(),
                                [](base::string16& left, Label* right) {
                                  return left + right->GetText();
                                }));
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, private:

void MessageBoxView::ResetLayoutManager() {
  // Initialize the Grid Layout Manager used for this dialog box.
  GridLayout* layout = SetLayoutManager(std::make_unique<views::GridLayout>());

  // Add the column set for the message displayed at the top of the dialog box.
  constexpr int kMessageViewColumnSetId = 0;
  ColumnSet* column_set = layout->AddColumnSet(kMessageViewColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::ColumnSize::kFixed, message_width_, 0);

  const LayoutProvider* provider = LayoutProvider::Get();

  // Column set for extra elements, if any.
  constexpr int kExtraViewColumnSetId = 1;
  if (prompt_field_->GetVisible() || checkbox_->GetVisible() ||
      link_->GetVisible()) {
    auto horizontal_insets = GetHorizontalInsets(provider);
    column_set = layout->AddColumnSet(kExtraViewColumnSetId);
    column_set->AddPaddingColumn(0, horizontal_insets.left());
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::ColumnSize::kUsePreferred, 0, 0);
    column_set->AddPaddingColumn(0, horizontal_insets.right());
  }

  layout->StartRow(0, kMessageViewColumnSetId);
  layout->AddExistingView(scroll_view_);

  views::DialogContentType trailing_content_type = views::TEXT;
  if (prompt_field_->GetVisible()) {
    layout->AddPaddingRow(0, inter_row_vertical_spacing_);
    layout->StartRow(0, kExtraViewColumnSetId);
    layout->AddExistingView(prompt_field_);
    trailing_content_type = views::CONTROL;
  }

  if (checkbox_->GetVisible()) {
    layout->AddPaddingRow(0, inter_row_vertical_spacing_);
    layout->StartRow(0, kExtraViewColumnSetId);
    layout->AddExistingView(checkbox_);
    trailing_content_type = views::TEXT;
  }

  if (link_->GetVisible()) {
    layout->AddPaddingRow(0, inter_row_vertical_spacing_);
    layout->StartRow(0, kExtraViewColumnSetId);
    layout->AddExistingView(link_);
    trailing_content_type = views::TEXT;
  }

  gfx::Insets border_insets = provider->GetDialogInsetsForContentType(
      views::TEXT, trailing_content_type);
  // Horizontal insets have already been applied to the message contents and
  // controls as padding columns. Only apply the missing vertical insets.
  border_insets.Set(border_insets.top(), 0, border_insets.bottom(), 0);
  SetBorder(CreateEmptyBorder(border_insets));

  InvalidateLayout();
}

gfx::Insets MessageBoxView::GetHorizontalInsets(
    const LayoutProvider* provider) {
  gfx::Insets horizontal_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);
  horizontal_insets.Set(0, horizontal_insets.left(), 0,
                        horizontal_insets.right());
  return horizontal_insets;
}

BEGIN_METADATA(MessageBoxView, View)
END_METADATA

}  // namespace views

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/message_box_view.h"

#include <stddef.h>

#include <memory>
#include <numeric>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
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
bool IsParagraphSeparator(char16_t c) {
  return (c == 0x000A || c == 0x000D || c == 0x001C || c == 0x001D ||
          c == 0x001E || c == 0x0085 || c == 0x2029);
}

// Splits |text| into a vector of paragraphs.
// Given an example "\nabc\ndef\n\n\nhij\n", the split results should be:
// "", "abc", "def", "", "", "hij", and "".
void SplitStringIntoParagraphs(const std::u16string& text,
                               std::vector<std::u16string>* paragraphs) {
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

MessageBoxView::MessageBoxView(const std::u16string& message,
                               bool detect_directionality)
    : inter_row_vertical_spacing_(LayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL)),
      message_width_(kDefaultMessageWidth) {
  const LayoutProvider* provider = LayoutProvider::Get();

  auto horizontal_insets = GetHorizontalInsets(provider);

  auto message_contents =
      Builder<BoxLayoutView>()
          .SetOrientation(BoxLayout::Orientation::kVertical)
          // We explicitly set insets on the message contents instead of the
          // scroll view so that the scroll view borders are not capped by
          // dialog insets.
          .SetBorder(CreateEmptyBorder(GetHorizontalInsets(provider)));

  auto add_label = [&message_contents, this](
                       const std::u16string& text,
                       gfx::HorizontalAlignment alignment) {
    message_contents.AddChild(
        Builder<Label>()
            .SetText(text)
            .SetTextContext(style::CONTEXT_DIALOG_BODY_TEXT)
            .SetMultiLine(!text.empty())
            .SetAllowCharacterBreak(true)
            .SetHorizontalAlignment(alignment)
            .CustomConfigure(base::BindOnce(
                [](std::vector<raw_ptr<Label, VectorExperimental>>&
                       message_labels,
                   Label* message_label) {
                  message_labels.push_back(message_label);
                },
                std::ref(message_labels_))));
  };

  if (detect_directionality) {
    std::vector<std::u16string> texts;
    SplitStringIntoParagraphs(message, &texts);
    for (const auto& text : texts) {
      // Avoid empty multi-line labels, which have a height of 0.
      add_label(text, gfx::ALIGN_TO_HEAD);
    }
  } else {
    add_label(message, gfx::ALIGN_LEFT);
  }

  Builder<MessageBoxView>(this)
      .SetOrientation(BoxLayout::Orientation::kVertical)
      .AddChildren(
          Builder<ScrollView>()
              .CopyAddressTo(&scroll_view_)
              .ClipHeightTo(0, provider->GetDistanceMetric(
                                   DISTANCE_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT))
              .SetContents(std::move(message_contents)),
          // TODO(crbug.com/40185544): Remove this, this is in place temporarily
          // to be able to submit accessibility checks, but this focusable View
          // needs to add a name so that the screen reader knows what to
          // announce.
          Builder<Textfield>()
              .CopyAddressTo(&prompt_field_)
              .SetProperty(kSkipAccessibilityPaintChecks, true)
              .SetProperty(kMarginsKey, horizontal_insets)
              .SetAccessibleName(message)
              .SetVisible(false)
              .CustomConfigure(base::BindOnce([](Textfield* prompt_field) {
                prompt_field->GetViewAccessibility().SetIsIgnored(true);
              })),
          Builder<Checkbox>()
              .CopyAddressTo(&checkbox_)
              .SetProperty(kMarginsKey, horizontal_insets)
              .SetVisible(false),
          Builder<Link>()
              .CopyAddressTo(&link_)
              .SetProperty(kMarginsKey, horizontal_insets)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .SetVisible(false))
      .BuildChildren();

  // Don't enable text selection if multiple labels are used, since text
  // selection can't span multiple labels.
  if (message_labels_.size() == 1u)
    message_labels_[0]->SetSelectable(true);

  ResetLayoutManager();
}

MessageBoxView::~MessageBoxView() = default;

views::Textfield* MessageBoxView::GetVisiblePromptField() {
  return prompt_field_ && prompt_field_->GetVisible() ? prompt_field_.get()
                                                      : nullptr;
}

std::u16string MessageBoxView::GetInputText() {
  return prompt_field_ && prompt_field_->GetVisible() ? prompt_field_->GetText()
                                                      : std::u16string();
}

bool MessageBoxView::HasVisibleCheckBox() const {
  return checkbox_ && checkbox_->GetVisible();
}

bool MessageBoxView::IsCheckBoxSelected() {
  return checkbox_ && checkbox_->GetVisible() && checkbox_->GetChecked();
}

void MessageBoxView::SetCheckBoxLabel(const std::u16string& label) {
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

void MessageBoxView::SetLink(const std::u16string& text,
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

void MessageBoxView::SetPromptField(const std::u16string& default_prompt) {
  DCHECK(prompt_field_);
  if (prompt_field_->GetVisible() && prompt_field_->GetText() == default_prompt)
    return;
  prompt_field_->SetText(default_prompt);
  prompt_field_->SetVisible(true);
  prompt_field_->GetViewAccessibility().SetIsIgnored(false);
  // The same text visible in the message box is used as an accessible name for
  // the prompt. To prevent it from being announced twice, we hide the message
  // to ATs.
  scroll_view_->GetViewAccessibility().SetIsLeaf(true);
  ResetLayoutManager();
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, View overrides:

gfx::Size MessageBoxView::CalculatePreferredSize(
    const SizeBounds& available_size) const {
  return BoxLayoutView::CalculatePreferredSize(
      SizeBounds(message_width_, available_size.height()));
}

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
                                message_labels_.cend(), std::u16string(),
                                [](const std::u16string& left, Label* right) {
                                  return left + right->GetText();
                                }));
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// MessageBoxView, private:

void MessageBoxView::ResetLayoutManager() {
  SetBetweenChildSpacing(inter_row_vertical_spacing_);
  SetMinimumCrossAxisSize(message_width_);

  views::DialogContentType trailing_content_type =
      views::DialogContentType::kText;
  if (prompt_field_->GetVisible())
    trailing_content_type = views::DialogContentType::kControl;

  bool checkbox_is_visible = checkbox_->GetVisible();
  if (checkbox_is_visible)
    trailing_content_type = views::DialogContentType::kText;

  // Ignored views are not in the accessibility tree, but their children
  // still can be exposed. Leaf views have no accessible children.
  checkbox_->GetViewAccessibility().SetIsIgnored(!checkbox_is_visible);
  checkbox_->GetViewAccessibility().SetIsLeaf(!checkbox_is_visible);

  if (link_->GetVisible())
    trailing_content_type = views::DialogContentType::kText;

  const LayoutProvider* provider = LayoutProvider::Get();
  gfx::Insets border_insets = provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, trailing_content_type);
  // Horizontal insets have already been applied to the message contents and
  // controls as padding columns. Only apply the missing vertical insets.
  border_insets.set_left_right(0, 0);
  SetBorder(CreateEmptyBorder(border_insets));

  InvalidateLayout();
}

gfx::Insets MessageBoxView::GetHorizontalInsets(
    const LayoutProvider* provider) {
  gfx::Insets horizontal_insets =
      provider->GetInsetsMetric(views::INSETS_DIALOG);
  horizontal_insets.set_top_bottom(0, 0);
  return horizontal_insets;
}

BEGIN_METADATA(MessageBoxView)
END_METADATA

}  // namespace views

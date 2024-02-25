// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/link_fragment.h"

#include <string>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/link.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_utils.h"

namespace views {

LinkFragment::LinkFragment(const std::u16string& title,
                           int text_context,
                           int text_style,
                           LinkFragment* other_fragment)
    : Link(title, text_context, text_style),
      prev_fragment_(this),
      next_fragment_(this) {
  // Connect to the previous fragment if it exists.
  if (other_fragment) {
    Connect(other_fragment);
  }

  views::FocusRing::Install(this);
  views::FocusRing::Get(this)->SetHasFocusPredicate(
      base::BindRepeating([](const View* view) {
        const auto* v = views::AsViewClass<LinkFragment>(view);
        CHECK(v);
        return InvokeOnFragments(&LinkFragment::HasFocus, v);
      }));
}

LinkFragment::~LinkFragment() {
  Disconnect();
}

void LinkFragment::Connect(LinkFragment* other_fragment) {
  DCHECK(prev_fragment_ == this);
  DCHECK(next_fragment_ == this);
  DCHECK(other_fragment);

  next_fragment_ = other_fragment->next_fragment_;
  other_fragment->next_fragment_->prev_fragment_ = this;
  prev_fragment_ = other_fragment;
  other_fragment->next_fragment_ = this;
}

void LinkFragment::Disconnect() {
  DCHECK((prev_fragment_ != this) == (next_fragment_ != this));
  if (prev_fragment_ != this) {
    prev_fragment_->next_fragment_ = next_fragment_;
    next_fragment_->prev_fragment_ = prev_fragment_;
  }
}

bool LinkFragment::IsUnderlined() const {
  return GetEnabled() &&
         (HasFocus() || IsMouseHovered() || GetForceUnderline());
}

void LinkFragment::RecalculateFont() {
  // Check whether any link fragment should be underlined.
  const bool should_be_underlined =
      InvokeOnFragments(&LinkFragment::IsUnderlined, this);

  // If the style differs from the current one, update.
  if ((font_list().GetFontStyle() & gfx::Font::UNDERLINE) !=
      should_be_underlined) {
    InvokeOnFragments(
        [should_be_underlined](LinkFragment* fragment) {
          const int style = fragment->font_list().GetFontStyle();
          const int intended_style = should_be_underlined
                                         ? (style | gfx::Font::UNDERLINE)
                                         : (style & ~gfx::Font::UNDERLINE);
          fragment->Label::SetFontList(
              fragment->font_list().DeriveWithStyle(intended_style));
          fragment->SchedulePaint();
          views::FocusRing::Get(fragment)->SchedulePaint();
          return false;
        },
        this);
  }
}

BEGIN_METADATA(LinkFragment)
END_METADATA

}  // namespace views

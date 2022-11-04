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
  if (other_fragment)
    Connect(other_fragment);

  views::FocusRing::Install(this);
  views::FocusRing::Get(this)->SetHasFocusPredicate([](View* view) -> bool {
    auto* v = views::AsViewClass<LinkFragment>(view);
    DCHECK(v);
    if (v->HasFocus())
      return true;

    // Iterate through the loop of fragments until reaching the current fragment
    // to see if any of them are focused. If so, this fragment will also be
    // focused.
    for (LinkFragment* current_fragment = v->next_fragment_;
         current_fragment != v;
         current_fragment = current_fragment->next_fragment_) {
      if (current_fragment->HasFocus())
        return true;
    }
    return false;
  });
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
  bool should_be_underlined = IsUnderlined();
  for (LinkFragment* current_fragment = next_fragment_;
       !should_be_underlined && current_fragment != this;
       current_fragment = current_fragment->next_fragment_) {
    should_be_underlined = current_fragment->IsUnderlined();
  }

  // If the style differs from the current one, update.
  if ((font_list().GetFontStyle() & gfx::Font::UNDERLINE) !=
      should_be_underlined) {
    auto MaybeUpdateStyle = [should_be_underlined](LinkFragment* fragment) {
      const int style = fragment->font_list().GetFontStyle();
      const int intended_style = should_be_underlined
                                     ? (style | gfx::Font::UNDERLINE)
                                     : (style & ~gfx::Font::UNDERLINE);
      fragment->Label::SetFontList(
          fragment->font_list().DeriveWithStyle(intended_style));
      fragment->SchedulePaint();
    };
    MaybeUpdateStyle(this);
    views::FocusRing::Get(this)->SchedulePaint();
    for (LinkFragment* current_fragment = next_fragment_;
         current_fragment != this;
         current_fragment = current_fragment->next_fragment_) {
      MaybeUpdateStyle(current_fragment);
      views::FocusRing::Get(current_fragment)->SchedulePaint();
    }
  }
}

BEGIN_METADATA(LinkFragment, Link)
END_METADATA

}  // namespace views

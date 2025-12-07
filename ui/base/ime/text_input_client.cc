// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/text_input_client.h"

#include <iomanip>
#include <ios>
#include <ostream>
#include <string_view>

namespace ui {

TextInputClient::~TextInputClient() {
}

bool TextInputClient::CanInsertImage() {
  return false;
}

#if BUILDFLAG(IS_CHROMEOS)
void TextInputClient::ExtendSelectionAndReplace(
    size_t length_before_selection,
    size_t length_after_selection,
    const std::u16string_view replacement_string) {
  ExtendSelectionAndDelete(length_before_selection, length_after_selection);
  InsertText(std::u16string(replacement_string),
             InsertTextCursorBehavior::kMoveCursorAfterText);
}

std::optional<GrammarFragment> TextInputClient::GetGrammarFragmentAtCursor()
    const {
  return std::nullopt;
}

bool TextInputClient::ClearGrammarFragments(const gfx::Range& range) {
  return false;
}

bool TextInputClient::AddGrammarFragments(
    const std::vector<GrammarFragment>& fragments) {
  return false;
}

bool TextInputClient::SupportsAlwaysConfirmComposition() {
  return true;
}
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
ui::TextInputClient::EditingContext TextInputClient::GetTextEditingContext() {
  return {};
}
#endif

#if BUILDFLAG(IS_WIN)
std::ostream& operator<<(std::ostream& os, ui::IndexFromPointFlags flags) {
  switch (flags) {
    case ui::IndexFromPointFlags::kNone:
      os << "None";
      break;
    case ui::IndexFromPointFlags::kNearestToContainedPoint:
      os << "NearestToContainedPoint";
      break;
    case ui::IndexFromPointFlags::kNearestToUncontainedPoint:
      os << "NearestToUncontainedPoint";
      break;
    case ui::IndexFromPointFlags::kNearestToPoint:
      os << "NearestToPoint";
      break;
    default:
      using T = std::underlying_type_t<ui::IndexFromPointFlags>;
      static_assert(sizeof(T) <= sizeof(uint32_t));
      os << "Unknown(0x" << std::setfill('0') << std::setw(sizeof(T) * 2)
         << std::hex << static_cast<uint32_t>(flags) << ")";
      break;
  }
  return os;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace ui

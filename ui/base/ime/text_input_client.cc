// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/text_input_client.h"

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
ui::TextInputClient::EditingContext TextInputClient::GetTextEditingContext() {
  return {};
}
#endif

}  // namespace ui

// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/text_input_client.h"

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
    const base::StringPiece16 replacement_string) {
  ExtendSelectionAndDelete(length_before_selection, length_after_selection);
  InsertText(std::u16string(replacement_string),
             InsertTextCursorBehavior::kMoveCursorAfterText);
}

absl::optional<GrammarFragment> TextInputClient::GetGrammarFragmentAtCursor()
    const {
  return absl::nullopt;
}

bool TextInputClient::ClearGrammarFragments(const gfx::Range& range) {
  return false;
}

bool TextInputClient::AddGrammarFragments(
    const std::vector<GrammarFragment>& fragments) {
  return false;
}
#endif

#if BUILDFLAG(IS_WIN)
ui::TextInputClient::EditingContext TextInputClient::GetTextEditingContext() {
  return {};
}
#endif

}  // namespace ui

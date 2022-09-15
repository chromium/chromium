// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/text_input_client.h"

namespace ui {

TextInputClient::~TextInputClient() {
}

#if BUILDFLAG(IS_CHROMEOS)
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

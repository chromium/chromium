// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_GRAMMAR_FRAGMENT_H_
#define UI_BASE_IME_GRAMMAR_FRAGMENT_H_

#include <string>

#include "base/component_export.h"
#include "ui/gfx/range/range.h"

namespace ui {

// A struct represents a fragment of grammar edit suggestion.
struct COMPONENT_EXPORT(UI_BASE_IME_TYPES) GrammarFragment {
  GrammarFragment(const gfx::Range& range, const std::string& suggestion);
  GrammarFragment(const GrammarFragment& other);
  ~GrammarFragment();

  bool operator==(const GrammarFragment& other) const;
  bool operator!=(const GrammarFragment& other) const;

  // The range of the marker, visual indications such as underlining are
  // expected to show in this range.
  gfx::Range range;

  // The replacement text suggested by the grammar model.
  std::string suggestion;
};

}  // namespace ui

#endif  // UI_BASE_IME_GRAMMAR_FRAGMENT_H_

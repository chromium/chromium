// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/grammar_fragment.h"

namespace ui {

GrammarFragment::GrammarFragment(const gfx::Range& range,
                                 const std::string& suggestion,
                                 const std::string& original_text)
    : range(range), suggestion(suggestion), original_text(original_text) {}

GrammarFragment::GrammarFragment(const GrammarFragment& other) = default;

GrammarFragment::~GrammarFragment() = default;

bool GrammarFragment::operator==(const GrammarFragment& other) const {
  return range == other.range && suggestion == other.suggestion &&
         original_text == other.original_text;
}

bool GrammarFragment::operator!=(const GrammarFragment& other) const {
  return !(*this == other);
}

}  // namespace ui

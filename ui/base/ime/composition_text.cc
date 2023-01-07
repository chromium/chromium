// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/composition_text.h"

namespace ui {

CompositionText::CompositionText() = default;

CompositionText::CompositionText(const CompositionText& other) = default;

CompositionText::~CompositionText() = default;

bool CompositionText::operator==(const CompositionText& other) const {
  return text == other.text && ime_text_spans == other.ime_text_spans &&
         selection == other.selection;
}

bool CompositionText::operator!=(const CompositionText& other) const {
  return !(*this == other);
}

}  // namespace ui

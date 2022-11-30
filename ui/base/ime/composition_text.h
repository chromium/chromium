// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_COMPOSITION_TEXT_H_
#define UI_BASE_IME_COMPOSITION_TEXT_H_

#include <stddef.h>

#include <string>

#include "base/component_export.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/gfx/range/range.h"

namespace ui {

// A struct represents the status of an ongoing composition text.
struct COMPONENT_EXPORT(UI_BASE_IME_TYPES) CompositionText {
  CompositionText();
  CompositionText(const CompositionText& other);
  ~CompositionText();

  bool operator==(const CompositionText& other) const;
  bool operator!=(const CompositionText& other) const;

  // Content of the composition text.
  std::u16string text;

  // ImeTextSpan information for the composition text.
  ImeTextSpans ime_text_spans;

  // Selection range in the composition text. It represents the caret position
  // if the range length is zero. Usually it's used for representing the target
  // clause (on Windows). Gtk doesn't have such concept, so background color is
  // usually used instead.
  gfx::Range selection;
};

}  // namespace ui

#endif  // UI_BASE_IME_COMPOSITION_TEXT_H_

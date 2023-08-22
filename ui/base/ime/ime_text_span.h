// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IME_TEXT_SPAN_H_
#define UI_BASE_IME_IME_TEXT_SPAN_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {

struct COMPONENT_EXPORT(UI_BASE_IME_TYPES) ImeTextSpan {
  enum class Type {
    // Creates a composition marker.
    kComposition,
    // Creates a suggestion marker that isn't cleared after the user picks a
    // replacement.
    kSuggestion,
    // Creates a suggestion marker that is cleared after the user picks a
    // replacement, and will be ignored if added to an element with spell
    // checking disabled.
    kMisspellingSuggestion,
    // Creates an autocorrect marker that isn't cleared by itself.
    kAutocorrect,
    // Creates a grammar marker that isn't cleared by itself.
    kGrammarSuggestion,
  };

  enum class Thickness {
    kNone,
    kThin,
    kThick,
  };

  enum class UnderlineStyle {
    kNone,
    kSolid,
    kDot,
    kDash,
    kSquiggle,
  };

  explicit ImeTextSpan(
      Type type = Type::kComposition,
      size_t start_offset = 0,
      size_t end_offset = 0,
      Thickness thickness = Thickness::kThin,
      UnderlineStyle underline_style = UnderlineStyle::kSolid,
      SkColor background_color = SK_ColorTRANSPARENT,
      SkColor suggestion_highlight_color = SK_ColorTRANSPARENT,
      const std::vector<std::string>& suggestions = std::vector<std::string>(),
      SkColor text_color = SK_ColorTRANSPARENT);

  ImeTextSpan(const ImeTextSpan& rhs);

  ~ImeTextSpan();

  bool operator==(const ImeTextSpan& rhs) const {
    return (this->type == rhs.type) &&
           (this->start_offset == rhs.start_offset) &&
           (this->end_offset == rhs.end_offset) &&
           (this->underline_color == rhs.underline_color) &&
           (this->thickness == rhs.thickness) &&
           (this->underline_style == rhs.underline_style) &&
           (this->text_color == rhs.text_color) &&
           (this->background_color == rhs.background_color) &&
           (this->suggestion_highlight_color ==
            rhs.suggestion_highlight_color) &&
           (this->remove_on_finish_composing ==
            rhs.remove_on_finish_composing) &&
           (this->interim_char_selection == rhs.interim_char_selection) &&
           (this->suggestions == rhs.suggestions);
  }

  bool operator!=(const ImeTextSpan& rhs) const { return !(*this == rhs); }

  Type type;
  size_t start_offset;
  size_t end_offset;
  SkColor underline_color = SK_ColorTRANSPARENT;
  Thickness thickness;
  UnderlineStyle underline_style;
  SkColor text_color;
  SkColor background_color;
  SkColor suggestion_highlight_color;
  bool remove_on_finish_composing = false;
  bool interim_char_selection = false;
  std::vector<std::string> suggestions;
};

typedef std::vector<ImeTextSpan> ImeTextSpans;

}  // namespace ui

#endif  // UI_BASE_IME_IME_TEXT_SPAN_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

// Intentionally keep sync with blink::WebImeTextSpan defined in:
// third_party/WebKit/public/web/WebImeTextSpan.h

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
  };

  enum class Thickness {
    kNone,
    kThin,
    kThick,
  };

  ImeTextSpan(
      Type type = Type::kComposition,
      uint32_t start_offset = 0,
      uint32_t end_offset = 0,
      Thickness thickness = Thickness::kThin,
      SkColor background_color = SK_ColorTRANSPARENT,
      SkColor suggestion_highlight_color = SK_ColorTRANSPARENT,
      const std::vector<std::string>& suggestions = std::vector<std::string>());

  ImeTextSpan(const ImeTextSpan& rhs);

  ~ImeTextSpan();

  bool operator==(const ImeTextSpan& rhs) const {
    return (this->type == rhs.type) &&
           (this->start_offset == rhs.start_offset) &&
           (this->end_offset == rhs.end_offset) &&
           (this->underline_color == rhs.underline_color) &&
           (this->thickness == rhs.thickness) &&
           (this->background_color == rhs.background_color) &&
           (this->suggestion_highlight_color ==
            rhs.suggestion_highlight_color) &&
           (this->remove_on_finish_composing ==
            rhs.remove_on_finish_composing) &&
           (this->suggestions == rhs.suggestions);
  }

  bool operator!=(const ImeTextSpan& rhs) const { return !(*this == rhs); }

  Type type;
  uint32_t start_offset;
  uint32_t end_offset;
  SkColor underline_color = SK_ColorTRANSPARENT;
  Thickness thickness;
  SkColor background_color;
  SkColor suggestion_highlight_color;
  bool remove_on_finish_composing = false;
  std::vector<std::string> suggestions;
};

typedef std::vector<ImeTextSpan> ImeTextSpans;

}  // namespace ui

#endif  // UI_BASE_IME_IME_TEXT_SPAN_H_
